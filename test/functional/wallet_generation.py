#!/usr/bin/env python3
# Copyright (c) 2026 The Taler Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Staking and mining with more than one wallet.

The node stakes or mines with one wallet at a time. In 0.20.0 the staker bound
itself to whichever wallet loaded first and held it for the whole session, so a
wallet created or loaded later could never stake, and unloading the staking wallet
hung forever. This test settles that:

- a wallet created with createwallet, loaded after startup, stakes a block;
- starting staking or mining on one wallet replaces whatever else was running;
- unloading the wallet that is staking returns at once and stops staking;
- a timed staking session ends on time, and when its wallet is locked, and it
  locks the wallet when it ends;
- mining pays the wallet it was started for;
- -stakewallet and -minewallet pick the wallet at startup, and without them the
  first wallet is used, as before.
"""

import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error, wait_until

STAKE_AMOUNT = 2000
# See wallet_stake_reorg.py: the chain has to span more than the stake modifier's
# selection interval before any kernel can be found.
BLOCK_TIME_STEP = 300
SETUP_ROUNDS = 40
PASSPHRASE = "correct horse battery staple"


class WalletGenerationTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-stakegen=0']]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def advance(self, seconds):
        self.mocktime += seconds
        self.nodes[0].setmocktime(self.mocktime)

    def mine(self, count=1):
        self.nodes[0].generatetoaddress(count, self.mining_address)

    def generation(self):
        return self.nodes[0].getgenerate()

    def wait_for_mode(self, mode, wallet=None):
        def reached():
            state = self.generation()
            return state['mode'] == mode and (wallet is None or state.get('wallet') == wallet)
        wait_until(reached, timeout=30)

    def stake_until_block(self, wallet_rpc):
        """Advance time until the node stakes a block; return that block."""
        node = self.nodes[0]
        height = node.getblockcount()
        for _ in range(120):
            self.advance(120)
            node.syncwithvalidationinterfacequeue()
            if node.getblockcount() > height:
                break
        else:
            raise AssertionError("no proof-of-stake block was produced")
        return node.getblock(node.getbestblockhash(), 2)

    def run_test(self):
        node = self.nodes[0]
        self.mocktime = 1735689600
        node.setmocktime(self.mocktime)
        default = node.get_wallet_rpc('')

        self.log.info("Two wallets created after startup, funded and left to age")
        node.createwallet('alpha')
        node.createwallet('beta')
        alpha = node.get_wallet_rpc('alpha')
        beta = node.get_wallet_rpc('beta')
        self.mining_address = default.getnewaddress()
        for _ in range(SETUP_ROUNDS):
            self.advance(BLOCK_TIME_STEP)
            self.mine(2)
        default.sendtoaddress(alpha.getnewaddress(), STAKE_AMOUNT)
        default.sendtoaddress(beta.getnewaddress(), STAKE_AMOUNT)
        for _ in range(SETUP_ROUNDS // 2):
            self.advance(BLOCK_TIME_STEP)
            self.mine(2)
        assert_equal(self.generation()['mode'], 'off')

        self.log.info("A wallet loaded after startup stakes")
        alpha.setgeneratepos("1")
        state = self.generation()
        assert_equal(state['mode'], 'staking')
        assert_equal(state['wallet'], 'alpha')
        assert_equal(state['until'], 0)
        assert_equal(state['from_config'], False)
        block = self.stake_until_block(alpha)
        assert_greater_than(len(block['tx']), 1)
        coinstake = block['tx'][1]['txid']
        alpha.gettransaction(coinstake)  # the stake is alpha's...
        assert_raises_rpc_error(-5, "Invalid or non-wallet transaction id", beta.gettransaction, coinstake)  # ...not beta's
        alpha.setgeneratepos("0")
        assert_equal(self.generation()['mode'], 'off')

        self.log.info("Only one wallet at a time: each start replaces what was running")
        alpha.setgeneratepos("1")
        beta.setgeneratepos("1")
        assert_equal(self.generation()['wallet'], 'beta')
        alpha.setgeneratepow("1")
        state = self.generation()
        assert_equal(state['mode'], 'mining')
        assert_equal(state['wallet'], 'alpha')
        assert_equal(state['minerPOSCount'], 0)
        beta.setgeneratepos("1")
        state = self.generation()
        assert_equal(state['mode'], 'staking')
        assert_equal(state['minerPOWCount'], 0)
        # Stopping the activity that is not running changes nothing.
        beta.setgeneratepow("0")
        assert_equal(self.generation()['mode'], 'staking')
        beta.setgeneratepos("0")

        self.log.info("Unloading the staking wallet returns and stops staking")
        alpha.setgeneratepos("1")
        self.advance(120)
        node.unloadwallet('alpha')  # hung forever in 0.20.0
        assert_equal(self.generation()['mode'], 'off')
        node.loadwallet('alpha')

        self.log.info("A timed session ends on time and locks the wallet")
        beta.encryptwallet(PASSPHRASE)
        locked_address = beta.getnewaddress()
        beta.walletpassphrase(PASSPHRASE, 100000)
        beta.setgeneratepos("1", "600")
        state = self.generation()
        assert_equal(state['until'], self.mocktime + 600)
        self.advance(601)
        self.wait_for_mode('off')
        assert_raises_rpc_error(-13, "walletpassphrase", beta.dumpprivkey, locked_address)

        self.log.info("A timed session ends when its wallet is locked")
        beta.walletpassphrase(PASSPHRASE, 100000)
        beta.setgeneratepos("1", "3600")
        beta.walletlock()
        self.wait_for_mode('off')

        self.log.info("An untimed session waits out a locked wallet, as -stakegen always has")
        beta.setgeneratepos("1")
        self.advance(120)
        time.sleep(3)  # a few supervisor passes, each of which finds the wallet locked
        assert_equal(self.generation()['mode'], 'staking')
        beta.setgeneratepos("0")

        self.log.info("Mining pays the wallet it was started for, and the wallet can stay locked")
        before = len(beta.listtransactions("*", 1000))
        beta.setgeneratepow("1")
        height = node.getblockcount()
        wait_until(lambda: node.getblockcount() > height, timeout=120)
        beta.setgeneratepow("0")
        assert_equal(self.generation()['mode'], 'off')
        mined = [t for t in beta.listtransactions("*", 1000)[-5:] if t['category'] in ('immature', 'generate')]
        assert mined, "the mined block did not pay the wallet mining was started for"
        assert_greater_than(len(beta.listtransactions("*", 1000)), before)

        self.log.info("-stakewallet picks the staking wallet at startup")
        self.restart_node(0, extra_args=['-stakegen=1', '-stakewallet=alpha', '-wallet=', '-wallet=alpha', '-wallet=beta'])
        node = self.nodes[0]
        node.setmocktime(self.mocktime)
        state = self.generation()
        assert_equal(state['mode'], 'staking')
        assert_equal(state['wallet'], 'alpha')
        assert_equal(state['from_config'], True)

        self.log.info("Without it, the first wallet stakes, as before")
        self.restart_node(0, extra_args=['-stakegen=1', '-wallet=alpha', '-wallet='])
        node = self.nodes[0]
        node.setmocktime(self.mocktime)
        assert_equal(self.generation()['wallet'], 'alpha')

        self.log.info("-minewallet picks the wallet -gen mines to")
        self.restart_node(0, extra_args=['-gen=1', '-minewallet=beta', '-wallet=', '-wallet=alpha', '-wallet=beta'])
        node = self.nodes[0]
        node.setmocktime(self.mocktime)
        state = self.generation()
        assert_equal(state['mode'], 'mining')
        assert_equal(state['wallet'], 'beta')
        node.get_wallet_rpc('beta').setgeneratepow("0")
        assert_equal(self.generation()['mode'], 'off')


if __name__ == '__main__':
    WalletGenerationTest().main()
