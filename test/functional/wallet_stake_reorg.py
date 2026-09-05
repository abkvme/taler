#!/usr/bin/env python3
# Copyright (c) 2026 The Taler Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""A stake block that loses a reorg must never cost the wallet coins.

A proof-of-stake block pays its staker through the coinbase, while the coinstake
transaction beside it returns the staked principal. When such a block is reorged
away the coinstake is left unconfirmed with the staked coin still marked spent,
and the question this test settles is whether the wallet ever gets stuck in that
state.

It must not, by either of two routes: the coinstake goes back to the mempool and
is mined again - its block commitment is a plain 36-byte OP_RETURN, so nothing
about it is unrelayable - or, failing that, it is released and the coin returns
to the unspent set. Either way the reported balance has to keep agreeing with
the wallet's own UTXOs at every step, which is the invariant asserted throughout
and the one a user would notice breaking.

The other half of the test matters just as much: an ordinary payment that shared
the orphaned block must not be abandoned. It can be mined again, and freeing its
inputs would let the wallet spend coins out from under a live transaction.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than

# CreateCoinStake ignores anything smaller than this.
MIN_STAKE_VALUE = 10
STAKE_AMOUNT = 2000
# The kernel needs the tip's block time to be well past the coin's, so the chain
# has to span more than GetStakeModifierSelectionInterval() - about 38 minutes
# with regtest's 60-second nStakeModifierInterval.
BLOCK_TIME_STEP = 300
SETUP_ROUNDS = 40
# The wallet only looks for stranded transactions once a minute, and only at ones
# older than fifteen. Both are wall-clock against the node's adjusted time.
STRANDED_TX_MIN_AGE = 15 * 60


class WalletStakeReorgTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        # Staking is driven explicitly here; a background staker would race
        # every assertion in this file.
        self.extra_args = [['-stakegen=0']]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def advance(self, seconds):
        """Move the node's clock forward and keep it there."""
        self.mocktime += seconds
        self.nodes[0].setmocktime(self.mocktime)

    def mine(self, count=1):
        self.nodes[0].generatetoaddress(count, self.mining_address)

    def build_chain(self):
        """A chain whose block times span long enough for the kernel to accept."""
        node = self.nodes[0]
        self.mining_address = node.getnewaddress()
        for _ in range(SETUP_ROUNDS):
            self.advance(BLOCK_TIME_STEP)
            self.mine(2)

        # One fat UTXO: the coinbase reward on regtest is far below the staking
        # minimum, so without consolidating there is nothing to stake with.
        node.sendtoaddress(node.getnewaddress(), STAKE_AMOUNT)
        for _ in range(SETUP_ROUNDS // 2):
            self.advance(BLOCK_TIME_STEP)
            self.mine(2)

    def stake_one_block(self):
        """Run the staker until it finds a block, then stop it again."""
        node = self.nodes[0]
        height = node.getblockcount()
        node.setgeneratepos("1")  # this RPC parses its argument as a string
        try:
            for _ in range(120):
                self.advance(120)
                if node.getblockcount() > height:
                    break
                self.nodes[0].syncwithvalidationinterfacequeue()
            else:
                raise AssertionError("the wallet never produced a proof-of-stake block")
        finally:
            node.setgeneratepos("0")

        block_hash = node.getbestblockhash()
        block = node.getblock(block_hash, 2)
        assert_greater_than(len(block['tx']), 1)
        return block_hash, block

    def run_test(self):
        node = self.nodes[0]
        self.mocktime = 1735689600  # a fixed starting point keeps runs comparable
        node.setmocktime(self.mocktime)

        self.log.info("Building a chain long enough in time for staking to be possible")
        self.build_chain()

        self.log.info("Staking a block")
        pos_hash, pos_block = self.stake_one_block()
        coinstake = pos_block['tx'][1]
        coinstake_txid = coinstake['txid']
        staked_input = coinstake['vin'][0]
        staked_outpoint = (staked_input['txid'], staked_input['vout'])
        self.log.info("Coinstake %s spends %s:%d", coinstake_txid, *staked_outpoint)

        # The staked coin is spent by the coinstake, so it is gone from the
        # unspent set while the stake block stands. That part is correct.
        assert staked_outpoint not in self.unspent_outpoints()

        balance_with_stake = node.getbalance()

        self.log.info("Reorging the stake block away")
        node.invalidateblock(pos_hash)
        self.nodes[0].syncwithvalidationinterfacequeue()

        self.log.info("The coinstake is unconfirmed and queued to be mined again, not abandoned")
        assert_equal(node.gettransaction(coinstake_txid)['confirmations'], 0)
        assert coinstake_txid in node.getrawmempool(), \
            "the coinstake was dropped instead of being re-queued, which strands the staked coin"
        self.assert_balance_matches_utxos("coinstake unconfirmed")

        # A real reorg happens because a competing chain won, so put the tip back
        # where it was. The wallet deliberately leaves coins alone while the
        # active chain has not yet passed the orphaned block.
        self.advance(BLOCK_TIME_STEP)
        self.mine(1)
        assert node.getbestblockhash() != pos_hash
        assert_equal(node.getblockcount(), pos_block['height'])

        self.log.info("Waiting out the stranded-transaction sweep")
        self.advance(STRANDED_TX_MIN_AGE + 120)
        self.mine(1)
        self.nodes[0].syncwithvalidationinterfacequeue()

        self.log.info("Neither route may lose the coin")
        self.assert_coin_not_lost(coinstake_txid, staked_outpoint)

        self.log.info("...and the same holds after a restart")
        self.restart_node(0, extra_args=['-stakegen=0'])
        node = self.nodes[0]
        node.setmocktime(self.mocktime)
        self.assert_coin_not_lost(coinstake_txid, staked_outpoint)

        self.log.info("An ordinary payment from a reorged block is left alone")
        self.check_payment_survives_reorg()

    def unspent_outpoints(self):
        return {(u['txid'], u['vout']) for u in self.nodes[0].listunspent(0, 9999999)}

    def assert_balance_matches_utxos(self, when):
        """The invariant a user would actually notice breaking.

        getbalance sums cached per-transaction credits; listunspent walks the
        wallet's outputs and asks IsSpent directly. They are computed by different
        code from different state, so they only agree when the wallet's idea of
        what it owns is coherent. Abandoning a transaction without invalidating
        those caches is precisely how they come apart.
        """
        node = self.nodes[0]
        balance = node.getbalance()
        utxos = sum((u['amount'] for u in node.listunspent(0, 9999999)), Decimal(0))
        assert_equal(balance, utxos)

    def assert_coin_not_lost(self, coinstake_txid, staked_outpoint):
        """Either the coinstake stands again, or the coin is back. Never neither."""
        node = self.nodes[0]
        entry = node.gettransaction(coinstake_txid)
        in_mempool = coinstake_txid in node.getrawmempool()
        unspent = {(u['txid'], u['vout']): u for u in node.listunspent(0, 9999999)}

        if entry['confirmations'] > 0 or in_mempool:
            # The coinstake lives, so the staked coin is legitimately spent and
            # its value came back as the coinstake's own outputs.
            assert staked_outpoint not in unspent
        else:
            # The coinstake is dead, so the coin it spent must be ours again -
            # otherwise it is stranded: gone from the balance and unable to stake.
            assert staked_outpoint in unspent, \
                "coinstake is dead and the staked coin did not come back: %s:%d" % staked_outpoint
            recovered = unspent[staked_outpoint]
            assert_greater_than(recovered['amount'], Decimal(MIN_STAKE_VALUE))
            assert recovered['spendable'], "the recovered coin cannot be staked again"

        self.assert_balance_matches_utxos("after the sweep")

    def check_payment_survives_reorg(self):
        """A payment in a reorged block goes back to the mempool, not to abandoned.

        This is the restraint half of the rule. Abandoning it would free its
        inputs while the transaction is still perfectly mineable, which is how a
        wallet talks itself into double-spending its own payment.
        """
        node = self.nodes[0]
        payment = node.sendtoaddress(node.getnewaddress(), 5)
        self.advance(BLOCK_TIME_STEP)
        self.mine(1)
        block_hash = node.getbestblockhash()
        assert_equal(node.gettransaction(payment)['confirmations'], 1)

        node.invalidateblock(block_hash)
        self.nodes[0].syncwithvalidationinterfacequeue()

        # Straight back to the mempool, exactly as upstream does it.
        assert payment in node.getrawmempool(), "a reorged payment was dropped instead of re-queued"
        assert_equal(node.gettransaction(payment)['confirmations'], 0)

        # And the sweep must not touch it, however old it gets: it has no block
        # of its own to be bound to.
        self.advance(STRANDED_TX_MIN_AGE + 120)
        self.mine(1)
        self.nodes[0].syncwithvalidationinterfacequeue()
        entry = node.gettransaction(payment)
        assert entry['confirmations'] >= 1 or payment in node.getrawmempool(), \
            "the sweep abandoned a payment that could still be mined"
        self.assert_balance_matches_utxos("after a payment reorg")


if __name__ == '__main__':
    WalletStakeReorgTest().main()
