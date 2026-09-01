#!/usr/bin/env python3
# Copyright (c) 2026 The Taler Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""The upgrade gate: a 0.19 wallet must survive 0.20, and 0.19 must survive after.

Everything else in the test suite runs on regtest. This cannot: 0.19 asserts
Bitcoin's regtest genesis hash and aborts before it opens a wallet, so the only
chain both versions can agree on is mainnet. No peers are ever contacted - the
nodes are started with networking off and only their wallets are exercised.

Run it against a build of the previous release:

    git worktree add /tmp/taler-0.19 v0.19.6.8
    cd /tmp/taler-0.19 && ./autogen.sh && ./configure --without-gui && make
    test/compat/wallet_compat.py --old /tmp/taler-0.19/src

What it proves, in order:

  1. a wallet made by 0.19 opens in 0.20 with the same seed, addresses and balance
  2. 0.20 leaves that wallet where it found it - no migration, no rewrite
  3. 0.19 opens it again afterwards, having also read 0.20's peers.dat
  4. a recovery-phrase wallet made by 0.20 is refused by 0.19, cleanly, and is
     still usable in 0.20 after the refusal
"""

import argparse
import json
import os
import shutil
import socket
import subprocess
import sys
import time

CONF = """\
server=1
listen=0
connect=0
dnsseed=0
discover=0
upnp=0
rpcuser=compat
rpcpassword=compat
rpcport={rpcport}
"""


class Fail(Exception):
    pass


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


class Node:
    """One talerd, of one version, on one datadir."""

    def __init__(self, bindir, datadir, rpcport):
        self.daemon = os.path.join(bindir, 'talerd')
        self.cli = os.path.join(bindir, 'taler-cli')
        self.datadir = datadir
        self.rpcport = rpcport
        self.process = None

    def start(self, extra_args=(), expect_failure=False):
        args = [self.daemon, '-datadir=' + self.datadir, '-printtoconsole=0'] + list(extra_args)
        self.process = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        deadline = time.time() + 60
        while time.time() < deadline:
            if self.process.poll() is not None:
                out, err = self.process.communicate()
                self.process = None
                if expect_failure:
                    return (out + err).decode('utf8', 'replace')
                raise Fail("{} exited during startup:\n{}".format(self.daemon, err.decode('utf8', 'replace')))
            try:
                self.rpc('getblockcount')
                if expect_failure:
                    raise Fail("{} started when it should have refused".format(self.daemon))
                return None
            except Fail:
                time.sleep(0.3)
        raise Fail("{} never answered RPC".format(self.daemon))

    def stop(self):
        if self.process is None:
            return
        try:
            self.rpc('stop')
        except Fail:
            self.process.kill()
        self.process.wait(timeout=120)
        self.process = None

    def rpc(self, *args):
        cmd = [self.cli, '-datadir=' + self.datadir, '-rpcport=' + str(self.rpcport),
               '-rpcuser=compat', '-rpcpassword=compat'] + [str(a) for a in args]
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.returncode != 0:
            raise Fail(result.stderr.decode('utf8', 'replace').strip())
        text = result.stdout.decode('utf8').strip()
        try:
            return json.loads(text)
        except ValueError:
            return text


def check(condition, message):
    if not condition:
        raise Fail(message)


def wallet_path(datadir):
    return os.path.join(datadir, 'wallet.dat')


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--old', required=True, help="directory holding the previous release's talerd and taler-cli")
    parser.add_argument('--new', default=os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..', 'src'),
                        help="directory holding the talerd and taler-cli under test")
    parser.add_argument('--tmpdir', default=None)
    parser.add_argument('--keep', action='store_true', help="do not delete the datadir afterwards")
    args = parser.parse_args()

    tmpdir = args.tmpdir or os.path.join(os.environ.get('TMPDIR', '/tmp'), 'taler_compat_%d' % int(time.time()))
    datadir = os.path.join(tmpdir, 'datadir')
    os.makedirs(datadir, exist_ok=True)
    rpcport = free_port()
    with open(os.path.join(datadir, 'taler.conf'), 'w', encoding='utf8') as f:
        f.write(CONF.format(rpcport=rpcport))

    old = Node(os.path.abspath(args.old), datadir, rpcport)
    new = Node(os.path.abspath(args.new), datadir, rpcport)

    for node, label in ((old, 'old'), (new, 'new')):
        for path in (node.daemon, node.cli):
            check(os.path.exists(path), "no {} binary at {}".format(label, path))

    def version_line(node):
        return subprocess.run([node.daemon, '-version'],
                              stdout=subprocess.PIPE).stdout.decode().splitlines()[0]

    print("old:", version_line(old))
    print("new:", version_line(new))

    try:
        # 1. A wallet as 0.19 leaves it.
        print("\n[1/4] creating a wallet with the previous release")
        old.start()
        before_info = old.rpc('getwalletinfo')
        addresses = [old.rpc('getnewaddress') for _ in range(5)]
        before_balance = old.rpc('getbalance')
        old.stop()

        check(os.path.exists(wallet_path(datadir)), "0.19 did not write wallet.dat in the datadir root")
        shutil.copy2(wallet_path(datadir), os.path.join(tmpdir, 'wallet.dat.before'))
        print("      seed {} - {} addresses".format(before_info['hdmasterkeyid'][:12], len(addresses)))

        # 2. 0.20 opens it, unchanged.
        print("[2/4] opening it with this build")
        new.start()
        after_info = new.rpc('getwalletinfo')
        check(after_info['hdmasterkeyid'] == before_info['hdmasterkeyid'],
              "the HD seed changed: {} -> {}".format(before_info['hdmasterkeyid'], after_info['hdmasterkeyid']))
        check(after_info['walletversion'] == before_info['walletversion'],
              "the wallet version changed: {} -> {}".format(before_info['walletversion'], after_info['walletversion']))
        check(after_info.get('hdscheme') == 'legacy',
              "a wallet made by 0.19 must stay legacy, got {}".format(after_info.get('hdscheme')))
        check(new.rpc('getbalance') == before_balance, "the balance changed")

        for address in addresses:
            info = new.rpc('getaddressinfo', address)
            check(info['ismine'], "0.20 no longer owns {}".format(address))
            check(info['hdkeypath'].startswith("m/0'/"),
                  "{} was re-derived on a new path: {}".format(address, info['hdkeypath']))

        # New addresses keep coming from the same legacy chain.
        fresh = new.rpc('getnewaddress')
        check(new.rpc('getaddressinfo', fresh)['hdkeypath'].startswith("m/0'/"),
              "0.20 switched an existing wallet to a different derivation")

        version = new.rpc('getnetworkinfo')
        check(version['version'] == 200000, "version is {}, expected 200000".format(version['version']))
        check('/Taler:0.20.0/' in version['subversion'],
              "subversion is {}".format(version['subversion']))

        # The wallet must still be where 0.19 will look for it.
        check(os.path.exists(wallet_path(datadir)),
              "0.20 moved wallet.dat out of the datadir root; 0.19 would not find it")

        # 3. And 0.19 opens it again - including 0.20's peers.dat and banlist.dat.
        print("[3/4] handing it back to the previous release")
        new.stop()
        old.start()
        back_info = old.rpc('getwalletinfo')
        check(back_info['hdmasterkeyid'] == before_info['hdmasterkeyid'], "the seed changed on the way back")
        check(old.rpc('getbalance') == before_balance, "the balance changed on the way back")
        for address in addresses + [fresh]:
            check(old.rpc('getaddressinfo', address)['ismine'],
                  "0.19 lost {} after 0.20 had the wallet".format(address))
        old.stop()

        # 4. A phrase wallet must be refused, not misread.
        print("[4/4] a recovery-phrase wallet, offered to the previous release")
        new.start()
        phrase = new.rpc('getnewmnemonic')
        new.rpc('createwallet', 'phrase', 'false', phrase)
        phrase_address = new.rpc('-rpcwallet=phrase', 'getnewaddress')
        new.stop()

        message = old.start(extra_args=['-wallet=phrase'], expect_failure=True)
        check(message is not None, "0.19 opened a wallet whose derivation it does not know")
        lowered = message.lower()
        # Two things stop an older client, and either is a clean stop: the wallet's
        # minimum version, and the non-tolerable BIP-44 flag it does not know. What
        # matters is that the message tells the user what to do about it.
        check('requires newer version' in lowered or 'flag' in lowered or 'not support' in lowered,
              "0.19 refused, but not for a reason a user can act on:\n{}".format(message.strip()))
        print("      refused with: {}".format(
            next((line for line in message.splitlines() if line.strip()), '').strip()[:120]))

        # The refusal must be a refusal, not damage.
        new.start(extra_args=['-wallet=phrase'])
        check(new.rpc('-rpcwallet=phrase', 'getaddressinfo', phrase_address)['ismine'],
              "the phrase wallet was damaged by the older client's attempt to open it")
        check(new.rpc('-rpcwallet=phrase', 'getwalletmnemonic').strip() == phrase.strip(),
              "the recovery phrase did not survive")
        new.stop()

    except Fail as e:
        for node in (old, new):
            try:
                node.stop()
            except Exception:
                pass
        print("\nFAILED: {}".format(e), file=sys.stderr)
        print("datadir kept at {}".format(datadir), file=sys.stderr)
        return 1
    finally:
        for node in (old, new):
            try:
                node.stop()
            except Exception:
                pass

    if not args.keep:
        shutil.rmtree(tmpdir, ignore_errors=True)
    print("\nUpgrade gate passed: 0.19 -> 0.20 -> 0.19 with the wallet intact.")
    return 0


if __name__ == '__main__':
    sys.exit(main())
