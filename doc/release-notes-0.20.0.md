Taler Core version 0.20.0
=========================

This release adds recovery-phrase wallets and multi-wallet support to the Taler
node and desktop wallet.

**Nothing changes on the network.** There is no consensus change, no protocol
change and no new message. A 0.20.0 node and a 0.19.x node speak to each other
exactly as two 0.19.x nodes do, and a 0.20.0 node can be rolled back to 0.19.x at
any time. Existing wallets keep working, keep their addresses, and are left where
they are on disk.

Upgrading
---------

Stop the node, replace the binaries, start it again. No reindex, no rescan, no
configuration change. Staking nodes keep staking with no change.

To go back to 0.19.x, replace the binaries again. The only thing 0.19.x cannot
open is a wallet you deliberately created from a recovery phrase in 0.20.0; it
refuses that wallet with a clear message rather than misreading it.

Recovery phrases
----------------

A wallet can now be created from - and restored from - a 24-word BIP-39 recovery
phrase. Write the words down and they are enough to rebuild the wallet on any
Taler wallet that follows the same derivation, including the mobile wallet.

The derivation is published at https://github.com/abkvme/taler.spec and is
BIP-39 + BIP-44 with SLIP-44 coin type **1524**:

    m/44'/1524'/0'/{0,1}/i

The node's test suite verifies itself against the vectors in that repository on
every CI run, so the node and any other wallet built to the spec derive the same
addresses from the same words.

### In the desktop wallet

* **Manage wallets** on the main screen: create, restore, rename, remove, and see
  where each wallet is stored. One wallet can be marked as the default, and that
  is the one that opens on start.
* Creating a wallet offers a recovery phrase, shows the words, and asks for a few
  of them back before continuing.
* An existing wallet's phrase can be shown again later, and stays sealed while the
  wallet is locked.
* Theme selector - follow the system, light, or dark - and translations for
  Russian, Ukrainian and Belarusian.

### Headless

    talerd -newwalletmnemonic=/path/to/phrase.txt

On a first start with no wallet, this creates the wallet from a freshly generated
phrase and writes the words to that file with mode 0600. It refuses to overwrite an
existing file, is ignored (with a log line) when a wallet already exists, and the
phrase never appears in `debug.log`. **Move that file to offline storage and delete
it from the machine.**

### New RPCs

| RPC | What it does |
| --- | --- |
| `getnewmnemonic` | Generate a 24-word phrase without creating anything |
| `createwallet "name" false "phrase" "passphrase"` | Create a wallet from a phrase, optionally encrypted from the start |
| `restorewallet "name" "phrase" ( birthday gap_limit "passphrase" )` | Recreate a wallet from a phrase and scan the chain for its history |
| `getwalletmnemonic` | Show this wallet's phrase (refused while locked) |
| `listwalletdir` | List the wallets on disk, loaded or not |

`getwalletinfo` gained `hdscheme` (`legacy` or `bip44`), `has_imported_keys`, and,
for phrase wallets, `coin_type` and `account`.

A restore derives 1000 addresses ahead on each chain by default and extends that
window automatically if it finds used addresses near the edge, so a wallet that
was used elsewhere before the restore is found in full. Pass a `birthday` to skip
scanning history from before the wallet existed. A restore is refused on a pruned
node, and while the node is still syncing, rather than reporting a balance that is
only part of the story.

### What is unchanged

* Wallets created before 0.20.0 keep the derivation they have (`m/0'/0'/k'`) and
  are never migrated, re-derived or moved. They have no recovery phrase and are
  still backed up by copying `wallet.dat`.
* Encrypting a phrase wallet does not rotate its seed, so the phrase keeps working.
  Encrypting a pre-0.20.0 wallet behaves exactly as it did before.
* `sethdseed` is refused on a phrase wallet: rotating the seed would strand the
  phrase. It is unchanged on other wallets.
* Importing a private key into a phrase wallet is allowed but flags the wallet as
  holding keys the phrase cannot recover (`has_imported_keys`).

Other changes
-------------

* Version numbers are now three components (`0.20.0`) rather than four.
* Dead DNS seeds removed: `dnsseed.talercrypto.com`, `talerseed2.vovanchik.net`
  and `talerseed3.vovanchik.net`.

Fixes
-----

These were found by building and running the test suite, which had never been
enabled in this fork:

* **Chain scan by timestamp could hang the node.** `CChain::FindEarliestAtLeast`
  had a binary search that stopped making progress once its window narrowed to two
  blocks, and spun forever. It is reached by `importmulti` and `importprivkey` with
  a timestamp, by `pruneblockchain`, and by a restore with a birthday.
* **Regtest could not start, and could not be mined.** It carried Bitcoin's
  genesis block, difficulty limits below the target its own blocks use, and no
  staking or difficulty parameters at all. Difficulty retargeting also ignored
  `fPowNoRetargeting`, so blocks minted instantly - as a test chain does - tripled
  the difficulty each time and the chain became unmineable within a dozen blocks.
  Regtest is a local-only test chain; mainnet and testnet are untouched.
* Several inherited test suites were repaired to match this fork's code, and the
  functional test framework was pointed at `taler.conf` and the `talerd` binary -
  it had been writing `bitcoin.conf`, which the node never reads.

Testing
-------

Tests are off by default in this fork's `configure`, which is why the suite had
gone unbuilt for so long. With them on, it builds and runs:

    ./configure --enable-tests && make
    ./src/test/test_bitcoin --run_test="$(grep -v '^#' test/unit-test-suites.txt | grep -v '^$' | paste -sd, -)"
    python3 test/functional/wallet_mnemonic.py
    python3 test/functional/wallet_mnemonic_startup.py

`test/compat/wallet_compat.py` runs the upgrade gate against a build of the
previous release: a 0.19 wallet through 0.20 and back to 0.19, and a phrase wallet
offered to 0.19. It runs in CI before a release is tagged.

Thirteen inherited unit suites still carry Bitcoin's keys, addresses or subsidy
schedule and are not built yet; they are listed with reasons in
`test/unit-test-suites.txt`.
