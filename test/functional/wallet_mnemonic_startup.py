#!/usr/bin/env python3
# Copyright (c) 2026 The Taler Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""The headless way to get a recovery-phrase wallet: -newwalletmnemonic=<file>.

An operator running talerd has no wizard, so the phrase is written once to a file
they name. Everything here is about that file being safe to rely on: it must be
readable only by its owner, it must never overwrite anything, the wallet must
match the phrase inside it, and the phrase must not leak into the debug log.

Also covers the protection that stops an older client from opening a wallet whose
derivation it does not understand.
"""

import os
import stat

from test_framework.test_framework import BitcoinTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import assert_equal


class WalletMnemonicStartupTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def phrase_file(self, name):
        return os.path.join(self.options.tmpdir, name)

    def clear_text_warning(self, name):
        """Creating a phrase wallet warns, loudly and on purpose: the words are
        sitting unencrypted on disk until the operator moves them."""
        return ("Warning: A new wallet was created from a recovery phrase. The phrase is "
                "stored IN CLEAR TEXT in {} - move it to safe offline storage and delete "
                "it from this machine.".format(self.phrase_file(name)))

    def run_test(self):
        node = self.nodes[0]
        datadir = node.datadir

        self.log.info("Without the flag, startup creates a legacy wallet as before")
        assert_equal(node.getwalletinfo()['hdscheme'], 'legacy')

        self.log.info("The flag is ignored when a wallet already exists")
        self.check_flag_ignored_with_existing_wallet(node)

        self.log.info("On a fresh datadir the flag creates a phrase wallet")
        phrase = self.check_creates_wallet_on_fresh_datadir(node)

        self.log.info("The phrase file is private to its owner")
        self.check_file_permissions()

        self.log.info("The wallet really is the one that phrase describes")
        self.check_wallet_matches_phrase(node, phrase)

        self.log.info("The phrase never reaches the debug log")
        self.check_phrase_not_logged(node, phrase)

        self.log.info("An existing file is never overwritten, and no wallet is made")
        self.check_refuses_to_overwrite(node)

    # ------------------------------------------------------------------ #

    def check_flag_ignored_with_existing_wallet(self, node):
        """A datadir that already holds a wallet is left alone."""
        target = self.phrase_file('ignored.txt')
        before = node.getwalletinfo()['hdmasterkeyid']

        self.restart_node(0, extra_args=['-newwalletmnemonic=' + target])

        assert not os.path.exists(target), "wrote a phrase file for a wallet it did not create"
        info = node.getwalletinfo()
        assert_equal(info['hdscheme'], 'legacy')
        assert_equal(info['hdmasterkeyid'], before)

        # The operator asked for something that did not happen, so it must be said
        # out loud rather than silently skipped.
        assert self.log_contains(0, 'newwalletmnemonic'), "no warning that the flag was ignored"

    def check_creates_wallet_on_fresh_datadir(self, node):
        """The case the flag exists for: nothing in the datadir yet."""
        target = self.phrase_file('created.txt')
        self.stop_node(0)
        self.wipe_wallets(node.datadir)
        self.start_node(0, extra_args=['-newwalletmnemonic=' + target])

        assert os.path.exists(target), "flag did not write the phrase file"
        phrase = self.read_phrase(target)
        assert_equal(len(phrase.split()), 24)

        info = self.nodes[0].getwalletinfo()
        assert_equal(info['hdscheme'], 'bip44')
        return phrase

    def check_file_permissions(self):
        """Anyone who can read this file owns the coins."""
        mode = stat.S_IMODE(os.stat(self.phrase_file('created.txt')).st_mode)
        assert_equal(mode & (stat.S_IRWXG | stat.S_IRWXO), 0)
        assert_equal(mode, 0o600)

    def check_phrase_not_logged(self, node, phrase):
        first_words = ' '.join(phrase.split()[:3])
        with open(os.path.join(node.datadir, 'regtest', 'debug.log'), encoding='utf8') as f:
            log = f.read()
        assert phrase not in log, "the recovery phrase was written to debug.log"
        assert first_words not in log, "part of the recovery phrase was written to debug.log"

    def check_refuses_to_overwrite(self, node):
        """A path that already exists is a mistake, not something to resolve."""
        target = self.phrase_file('existing.txt')
        with open(target, 'w', encoding='utf8') as f:
            f.write('do not lose me\n')

        self.stop_node(0, expected_stderr=self.clear_text_warning('created.txt'))
        self.wipe_wallets(node.datadir)
        node.assert_start_raises_init_error(
            extra_args=['-newwalletmnemonic=' + target],
            expected_msg='Refusing to overwrite it',
            match=ErrorMatch.PARTIAL_REGEX)

        # The existing file is untouched and no wallet was left behind.
        with open(target, encoding='utf8') as f:
            assert_equal(f.read(), 'do not lose me\n')
        assert not self.wallet_files(node.datadir), "a wallet was created despite the abort"

        # Leave a plain running node behind for the framework to shut down: started
        # without the flag it creates an ordinary wallet and says nothing on stderr.
        self.start_node(0)

    def check_wallet_matches_phrase(self, node, phrase):
        """The words on disk have to be the words that own the coins."""
        assert_equal(node.getwalletmnemonic().strip(), phrase.strip())

        # Restoring the phrase into a second wallet must reproduce the same
        # addresses: that is the whole promise the file makes to its operator.
        node.generate(30)  # a restore is refused while the node is still syncing
        original = [node.getnewaddress() for _ in range(3)]

        node.restorewallet('from_file', phrase)
        restored = node.get_wallet_rpc('from_file')
        assert_equal([restored.getnewaddress() for _ in range(3)], original)

    # ------------------------------------------------------------------ #

    def read_phrase(self, path):
        """The file carries comment lines for the operator; the phrase is the rest."""
        with open(path, encoding='utf8') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    return line
        raise AssertionError("no phrase line in " + path)

    def wallet_files(self, datadir):
        walletdir = os.path.join(datadir, 'regtest')
        found = []
        for root, _dirs, files in os.walk(walletdir):
            for name in files:
                if name == 'wallet.dat':
                    found.append(os.path.join(root, name))
        return found

    def wipe_wallets(self, datadir):
        import shutil
        for path in self.wallet_files(datadir):
            os.remove(path)
        wallets = os.path.join(datadir, 'regtest', 'wallets')
        if os.path.isdir(wallets):
            shutil.rmtree(wallets)
        for name in ('database', 'db.log'):
            path = os.path.join(datadir, 'regtest', name)
            if os.path.isdir(path):
                shutil.rmtree(path)
            elif os.path.exists(path):
                os.remove(path)

    def log_contains(self, i, needle):
        with open(os.path.join(self.nodes[i].datadir, 'regtest', 'debug.log'), encoding='utf8') as f:
            return needle in f.read()


if __name__ == '__main__':
    WalletMnemonicStartupTest().main()
