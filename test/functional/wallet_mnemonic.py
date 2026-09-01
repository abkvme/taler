#!/usr/bin/env python3
# Copyright (c) 2026 The Taler Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Recovery-phrase wallets: creation, restore, and the guarantees around them.

The addresses asserted here come from data/seed-derivation-v1.json, a copy of the
published vectors at https://github.com/abkvme/taler.spec that every Taler wallet
verifies against. If this test fails, the node and the mobile wallet no longer
derive the same addresses from the same phrase - the one thing the shared spec
exists to prevent.

Legacy wallets are covered too: the point of the feature is that it changes
nothing for a wallet that already exists.
"""

import json
import os
import stat

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, assert_greater_than

VECTORS = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'data', 'seed-derivation-v1.json')

# BIP-44 look-ahead: enough addresses to cover a wallet used elsewhere before restore.
DEFAULT_GAP_LIMIT = 1000


class WalletMnemonicTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def load_vectors(self):
        with open(VECTORS, encoding='utf8') as f:
            return json.load(f)

    def run_test(self):
        self.vectors = self.load_vectors()
        node = self.nodes[0]

        # A restore is refused while the node is still syncing, so give it a chain
        # first. 30 blocks also matures the first coinbase (maturity is 20 here),
        # which the funding check further down spends.
        node.generate(30)

        self.log.info("Legacy wallet is untouched by the feature")
        self.check_legacy_wallet_unchanged(node)

        self.log.info("A generated phrase is 24 words and round-trips")
        self.check_generated_phrase(node)

        self.log.info("Addresses match the published derivation vectors")
        self.check_vector_addresses(node)

        self.log.info("Restoring the same phrase reproduces the same wallet")
        self.check_restore_reproduces_addresses(node)

        self.log.info("getwalletinfo reports the scheme, coin type and account")
        self.check_wallet_info(node)

        self.log.info("Rejections: bad checksum, wrong word count, duplicate name")
        self.check_invalid_phrases(node)

        self.log.info("sethdseed is refused on a phrase wallet")
        self.check_sethdseed_refused(node)

        self.log.info("Encryption keeps the phrase and its addresses intact")
        self.check_encryption_preserves_derivation(node)

        self.log.info("An imported key marks the wallet as not fully recoverable")
        self.check_imported_keys_flagged(node)

        self.log.info("Restore finds funds beyond the first addresses")
        self.check_restore_finds_funds(node)

    # ------------------------------------------------------------------ #

    def check_legacy_wallet_unchanged(self, node):
        """The wallet loaded at startup was made the old way and stays that way."""
        info = node.getwalletinfo()
        assert_equal(info['hdscheme'], 'legacy')
        assert_equal(info['has_imported_keys'], False)
        # A legacy wallet has no phrase to show, and saying so is not an error state
        # the user can fix - it is simply not that kind of wallet.
        assert_raises_rpc_error(-4, "not created from a recovery phrase", node.getwalletmnemonic)

    def check_generated_phrase(self, node):
        phrase = node.getnewmnemonic()
        assert_equal(len(phrase.split()), 24)

        node.createwallet('generated', False, phrase)
        w = node.get_wallet_rpc('generated')
        assert_equal(w.getwalletmnemonic(), phrase)
        assert_equal(w.getwalletinfo()['hdscheme'], 'bip44')

    def check_vector_addresses(self, node):
        """First addresses of a fresh wallet, against the shared spec."""
        case = self.vectors['cases'][0]
        expected = [e['address'] for e in case['networks']['regtest']['chains']['external'][:5]]

        node.createwallet('vector', False, case['mnemonic'])
        w = node.get_wallet_rpc('vector')
        derived = [w.getnewaddress() for _ in range(5)]
        assert_equal(derived, expected)

        # Every one of them is a real address of this network, not just a string
        # that happens to match.
        for address in derived:
            assert_equal(w.getaddressinfo(address)['ismine'], True)

        # A second case, so a single hard-coded value cannot pass by accident.
        case2 = self.vectors['cases'][1]
        node.createwallet('vector2', False, case2['mnemonic'])
        w2 = node.get_wallet_rpc('vector2')
        expected2 = [e['address'] for e in case2['networks']['regtest']['chains']['external'][:3]]
        assert_equal([w2.getnewaddress() for _ in range(3)], expected2)

    def check_restore_reproduces_addresses(self, node):
        """Create here, restore there: the wallets must be identical."""
        case = self.vectors['cases'][2]
        node.createwallet('origin', False, case['mnemonic'])
        origin = node.get_wallet_rpc('origin')
        before = [origin.getnewaddress() for _ in range(5)]

        node.restorewallet('restored', case['mnemonic'])
        restored = node.get_wallet_rpc('restored')
        after = [restored.getnewaddress() for _ in range(5)]

        assert_equal(before, after)
        assert_equal(origin.getwalletmnemonic(),
                     restored.getwalletmnemonic())

    def check_wallet_info(self, node):
        w = node.get_wallet_rpc('vector')
        info = w.getwalletinfo()
        assert_equal(info['hdscheme'], 'bip44')
        assert_equal(info['has_imported_keys'], False)
        # Regtest derives under coin type 1, as every test chain does; mainnet uses
        # Taler's registered 1524. Getting this wrong would silently produce a
        # different wallet from the same phrase.
        assert_equal(info['coin_type'], 1)
        assert_equal(info['account'], 0)
        assert 'hdmasterkeyid' in info

    def check_invalid_phrases(self, node):
        case = self.vectors['cases'][0]
        words = case['mnemonic'].split()

        # Wrong checksum: last word replaced by another valid word.
        bad_checksum = ' '.join(words[:-1] + ['zoo' if words[-1] != 'zoo' else 'zone'])
        assert_raises_rpc_error(-8, "checksum", node.createwallet, 'bad1', False, bad_checksum)

        # Too few words.
        assert_raises_rpc_error(-8, "24", node.createwallet, 'bad2', False, ' '.join(words[:12]))

        # A word that is not in the list at all.
        not_a_word = ' '.join(words[:-1] + ['tallerman'])
        assert_raises_rpc_error(-8, "word", node.createwallet, 'bad3', False, not_a_word)

        # Restoring onto a name already in use must not touch the existing wallet.
        assert_raises_rpc_error(-4, "already exists", node.restorewallet, 'vector', case['mnemonic'])
        assert_equal(node.get_wallet_rpc('vector').getwalletinfo()['hdscheme'], 'bip44')

        # None of the rejected wallets were created.
        listed = [entry['name'] for entry in node.listwalletdir()['wallets']]
        for name in ('bad1', 'bad2', 'bad3'):
            assert name not in listed, "rejected phrase left {} behind".format(name)

    def check_sethdseed_refused(self, node):
        """Rotating the seed under a phrase wallet would strand the phrase."""
        w = node.get_wallet_rpc('vector')
        assert_raises_rpc_error(-4, "recovery phrase", w.sethdseed)

    def check_encryption_preserves_derivation(self, node):
        """Encrypting must not rotate the seed - the phrase has to keep working."""
        case = self.vectors['cases'][3]
        node.createwallet('encrypted', False, case['mnemonic'])
        w = node.get_wallet_rpc('encrypted')
        before = [w.getnewaddress() for _ in range(3)]

        w.encryptwallet('passphrase')
        w = node.get_wallet_rpc('encrypted')

        # Locked: the phrase is key material and stays sealed.
        assert_raises_rpc_error(-13, "passphrase", w.getwalletmnemonic)

        w.walletpassphrase('passphrase', 30)
        assert_equal(w.getwalletmnemonic(), case['mnemonic'])

        # The addresses derived before encryption are still this wallet's addresses.
        for address in before:
            assert_equal(w.getaddressinfo(address)['ismine'], True)
        assert_equal([w.getnewaddress() for _ in range(3)],
                     [e['address'] for e in case['networks']['regtest']['chains']['external'][3:6]])

        # And the phrase still restores the same wallet elsewhere.
        node.restorewallet('encrypted_restored', case['mnemonic'])
        restored = node.get_wallet_rpc('encrypted_restored')
        assert_equal([restored.getnewaddress() for _ in range(3)], before)

    def check_imported_keys_flagged(self, node):
        """A key that came from outside the phrase cannot be recovered by it.

        Importing is still allowed - people have keys - but the wallet has to say
        so, or its owner will believe the words alone are a complete backup.
        """
        case = self.vectors['cases'][0]
        node.createwallet('imported', False, case['mnemonic'])
        w = node.get_wallet_rpc('imported')
        assert_equal(w.getwalletinfo()['has_imported_keys'], False)

        # A key from another wallet entirely.
        legacy = node.get_wallet_rpc('')
        outside_key = legacy.dumpprivkey(legacy.getnewaddress())
        w.importprivkey(outside_key, 'from_elsewhere', False)

        assert_equal(w.getwalletinfo()['has_imported_keys'], True)

        # And it has to still say so after a restart - a flag that lives only in
        # memory would tell the owner the truth once and lie afterwards.
        self.restart_node(0)
        node.loadwallet('imported')
        assert_equal(node.get_wallet_rpc('imported').getwalletinfo()['has_imported_keys'], True)

    def check_restore_finds_funds(self, node):
        """Coins sent to an address well past the first few must survive a restore.

        This is what the look-ahead is for: a wallet used on a phone for a while
        has spent its early addresses, and a restore that only derived a handful
        would report an empty wallet holding real money.
        """
        case = self.vectors['cases'][1]
        funded_index = 150

        node.createwallet('deep', False, case['mnemonic'])
        deep = node.get_wallet_rpc('deep')
        addresses = [deep.getnewaddress() for _ in range(funded_index + 1)]
        target = addresses[funded_index]

        legacy = node.get_wallet_rpc('')
        legacy.sendtoaddress(target, 1)
        # generate is a wallet RPC: with several wallets loaded the node cannot
        # guess which one to mine to, so name the address explicitly.
        node.generatetoaddress(1, legacy.getnewaddress())
        assert_equal(deep.getbalance(), 1)

        # A fresh wallet from the same phrase, with no knowledge of how far the
        # original had gone, has to find it too.
        result = node.restorewallet('deep_restored', case['mnemonic'])
        restored = node.get_wallet_rpc('deep_restored')
        assert_equal(restored.getbalance(), 1)

        # And it reports what it did, so an operator can see how far it looked.
        assert_greater_than(result['gap_limit'], funded_index)
        assert_greater_than(result['used_addresses'], 0)


if __name__ == '__main__':
    WalletMnemonicTest().main()
