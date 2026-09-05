#!/usr/bin/env python3
# Copyright (c) 2026 The Taler Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""recoverwallet: find history a wallet never knew it had.

This is the cure for the balance that reads too low. A wallet only recognises
addresses it has already derived, so one that never scanned - restored from an
older backup, or locked while it scanned, because a locked wallet cannot extend
its address window - stays blind to everything used after that point, and an
ordinary rescan does not help because it looks for the same short list of
addresses.

The scenario here is the honest one: a wallet created from a phrase that already
has history, which is exactly the state that reports zero.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than, assert_raises_rpc_error


class WalletRecoverTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [['-stakegen=0']]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def bump_time_past_tip(self):
        """Move the clock past the tip's block time.

        Blocks mined back to back take their time from the previous block plus a
        second, so a run of them ends up ahead of the wall clock. Taler rejects a
        transaction timestamped before the block that created the coins it spends
        (bad-txns-spent-too-early), so spending needs the clock caught up first.
        """
        node = self.nodes[0]
        tip_time = node.getblock(node.getbestblockhash())['time']
        node.setmocktime(tip_time + 120)

    def run_test(self):
        node = self.nodes[0]
        # Once a second wallet is loaded, wallet RPCs need to name one, so the
        # funding wallet is addressed explicitly from here on.
        w0 = node.get_wallet_rpc('')
        mining_address = w0.getnewaddress()
        node.generatetoaddress(60, mining_address)
        self.bump_time_past_tip()

        phrase = node.getnewmnemonic()
        node.createwallet('funded', False, phrase)
        funded = node.get_wallet_rpc('funded')

        # Spend to a few of its addresses, at a spread of indices, so recovery has
        # to walk forward rather than getting lucky on the first one.
        addresses = [funded.getnewaddress() for _ in range(5)]
        for address in addresses:
            w0.sendtoaddress(address, 3)
        node.generatetoaddress(2, mining_address)
        self.bump_time_past_tip()
        expected = funded.getbalance()
        assert_equal(expected, Decimal('15'))
        self.log.info("Funded wallet holds %s", expected)

        # The same phrase, opened as a fresh wallet. createwallet marks it as
        # current from this block on, so it never looks back and reports nothing -
        # the state a user sees as "my balance is wrong".
        node.createwallet('blind', False, phrase)
        blind = node.get_wallet_rpc('blind')
        assert_equal(blind.getbalance(), Decimal('0'))
        self.log.info("Same phrase, unscanned wallet reports %s", blind.getbalance())

        result = blind.recoverwallet()
        self.log.info("recoverwallet: %s", result)
        assert_equal(blind.getbalance(), expected)
        assert_greater_than(result['gap_limit'], 0)
        assert_greater_than(result['passes'], 0)
        assert_equal(result['scanned_from_height'], 0)
        assert_equal(Decimal(str(result['balance'])), expected)

        self.log.info("Running it again is harmless and finds the same balance")
        blind.recoverwallet()
        assert_equal(blind.getbalance(), expected)

        self.log.info("It refuses a locked wallet rather than scanning for nothing")
        node.createwallet('locked', False, phrase)
        locked = node.get_wallet_rpc('locked')
        locked.encryptwallet('passphrase')
        locked = node.get_wallet_rpc('locked')
        # A locked wallet cannot derive the look-ahead, which is the whole point of
        # this RPC - so it must say so rather than scan and report an empty wallet.
        assert_raises_rpc_error(-13, "passphrase", locked.recoverwallet)

        locked.walletpassphrase('passphrase', 60)
        locked.recoverwallet()
        assert_equal(locked.getbalance(), expected)

        self.log.info("Bad arguments are rejected")
        assert_raises_rpc_error(-8, "gap_limit", blind.recoverwallet, 1)
        assert_raises_rpc_error(-8, "start_height", blind.recoverwallet, 100, 999999)


if __name__ == '__main__':
    WalletRecoverTest().main()
