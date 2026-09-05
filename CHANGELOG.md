# Taler 0.20.0

## Release Date
Unreleased - targeting September 2026

## Major Changes

### Recovery-Phrase Wallets (BIP-39 + BIP-44)
- Wallets can be created from, and restored from, a 24-word BIP-39 recovery phrase (English wordlist, no passphrase)
- Derivation is `m/44'/1524'/0'/{0,1}/i` - BIP-44 with SLIP-44 coin type 1524 (1 on testnet and regtest), hardened down to the account so an account xpub can derive receive addresses on its own
- Derivation is published as a spec with shared test vectors at https://github.com/abkvme/taler.spec; the node verifies itself against those vectors in CI, so any wallet built to the spec derives the same addresses from the same words
- The phrase entropy is stored in the wallet, encrypted with the wallet's master key when the wallet is encrypted, so the words can be shown again later
- Encrypting a phrase wallet no longer rotates the HD seed (as it does for legacy wallets), because rotating it would strand the phrase
- `sethdseed` is refused on a phrase wallet; unchanged on every other wallet
- Importing a private key into a phrase wallet is allowed but sets `has_imported_keys`, so its owner is told the words alone are no longer a complete backup
- The BIP-44 wallet flag is non-tolerable (bit 33): an older client refuses such a wallet rather than misreading it as legacy
- New RPCs: `getnewmnemonic`, `getwalletmnemonic`, `restorewallet`, `listwalletdir`; `createwallet` gained `mnemonic` and `passphrase` arguments
- `getwalletinfo` gained `hdscheme` (`legacy` or `bip44`), `has_imported_keys`, and `coin_type` / `account` for phrase wallets
- Restore derives 1000 addresses ahead per chain and extends the window automatically when used addresses appear near its edge, so a wallet used elsewhere before the restore is found in full
- Restore accepts an optional wallet birthday to skip scanning history from before the wallet existed
- Restore is refused on a pruned node, and while the node is still syncing, rather than reporting a balance that is only part of the story
- Headless: `-newwalletmnemonic=<file>` creates the wallet from a fresh phrase on a first start with no wallet, writing the words with mode 0600; it refuses to overwrite an existing file, is ignored with a log line when a wallet already exists, and never writes the phrase to `debug.log`
- Headless: `-createwalletonstart=0` lets the node start with no wallet at all (the GUI uses this so its creation wizard can genuinely be cancelled)

### Multi-Wallet Management
- Wallet selector and a Manage wallets button on the Overview page, above the balance, rather than buried in menus
- Manage wallets dialog lists every wallet on disk with its type (seed phrase or legacy), balance, default marker and absolute path, and can create, restore, rename, remove and set the default
- The default wallet is remembered and opened on start
- Wallet names are validated as directory names (latin letters, digits and `-`), and a name already in use is rejected
- Wallet operations run on a worker thread behind a modal progress dialog; `unloadwallet` blocks until every reference is released and deadlocked the GUI when called on its own thread
- The pre-0.20.0 wallet is shown as `legacy` and cannot be renamed, since it lives in the datadir root rather than in a directory of its own
- `listwalletdir` identifies wallets by file content (Berkeley DB magic) rather than by name, and reports which ones are loaded by comparing normalized wallet-file paths

### Home Screen: Rewards at a Glance
- Recent transactions moved under the Balances card, so the left column is balances and history together
- Two status badges at the top of the right column: how recently this wallet earned a staking reward, colour-coded (green under 7 days, amber 7 to 30, red beyond that or never), and whether a new staking round can be started
- The staking badge carries a quieter line for proof-of-work mining, shown only for wallets that have actually mined
- New rewards chart: one bar per month for the last 12 months, current month at the right, with the twelve-month total beside the title and a per-month tooltip
- The staking panel moved to the bottom of the right column, under the chart
- Rewards are read from the wallet's own transactions - a Taler proof-of-stake block pays its staker through the block's coinbase, so staking and mining rewards both arrive as generated transactions and are told apart by the block type. The coinstake transaction returning the staked principal is deliberately not counted as income
- The summary walks the transaction model's cached records directly rather than through the model's index(), which try-locks the wallet twice per row to refresh a status none of these fields depend on; a full pass over 100 000 transactions takes about 0.6 ms and takes no locks
- Recomputation is coalesced behind a 250 ms timer, so a rescan inserting thousands of rows triggers one pass rather than thousands
- Balance amounts align to the right edge of the Balances card, so a column of figures shares a right edge and can be read down its last digit
- The "no transactions yet" message now follows the list's own resizes, instead of keeping the width it had when first placed
- Staking rewards are listed in the application accent rather than a pure blue, which was close to unreadable on the dark theme
- Message-box icons (question, information, warning, error) are drawn in the application's own colours instead of the platform's grey, which all but disappeared against the dark theme
- Passphrase dialogs name the wallet they act on in their title: encrypt, unlock, decrypt and change-passphrase all apply to the wallet currently shown, and with several wallets open the title is the only thing that says which one

### Desktop Wallet Interface
- New start-up screen: the Taler artwork, in the language the interface is set to, with English as the fallback for any language that has no image of its own. The application version sits in the top-right corner, where all three images leave room, and testnet or regtest says so beneath it. Start-up progress moved to a strip under the artwork rather than on top of it - the strip takes its colour from the artwork's own bottom edge, so replacing the images later needs no code change
- Artwork is embedded as JPEG rather than PNG: these are photographic gradients, where PNG is the wrong format. 663KB for all three instead of 2.7MB, with nothing visible to tell them apart. That needs the Qt JPEG plugin, which a static build cannot load from disk, so configure now links it in - optionally, because the splash falls back to a plain panel and that is not worth failing a build over. The plugin lives in the Qt plugin tree's `imageformats` directory, which was not on the link path
- The language the splash picks and the language the translators are loaded for now come from one function, `GUIUtil::languageTerritory()`, instead of two copies that would eventually disagree
- **About** (from the menu) is two tabs now instead of a small logo beside a wall of licence text: the start-up artwork on the first, the licence on the second. They answer different questions, and stacking them served neither - the picture was squashed and the licence had to be read through a letterbox. The two URLs printed on the artwork are clickable, with transparent buttons sitting exactly over them: invisible at rest, a faint wash on hover so the printed pills read as live. Licence URLs are clickable too; they were being formatted as links but the label had no browser interaction flags, so they rendered as blue text that did nothing
- The shutdown window matches the start-up screen too, so a session opens and closes on the same face rather than ending in a bare dialog
- The artwork, the language choice, the fallback, the version placement and the clickable link positions live in one place (`brand::`), shared by the splash, the About dialog and the shutdown window, so the three cannot drift apart

- The start-up sync overlay ("recent transactions may not yet be visible") now follows the theme: its form hard-coded a near-white card with dark grey text, so it stayed white in dark mode, and its warning icon was a fixed pixmap. Colours moved into the theme with everything else, and the icon is drawn from the theme like every other message icon
- Theme selector in Settings: follow the system, light, or dark, applied without a restart; both themes are built from one stylesheet template and two colour sets so they cannot drift apart
- One card style across the whole application - same border, radius and surface for every panel, table and group box
- Available balance shown in an accent badge above the Balances card, to two decimals; the exact figure stays on the rows below
- Recovery phrase shown as a numbered 24-word grid, with a verification step before the wallet is created
- Overview and transaction views gained empty states, larger date and amount columns, and accent-coloured transaction icons
- Toolbar icons use the brand accent colour rather than a palette colour sampled once at startup, which made them invisible after a theme switch
- About page links to `explorer.taler.tech`
- Translations for the new interface in Russian, Ukrainian and Belarusian

#### Toolbar
- Split in two: the places money moves through on the left - Overview, Send, Receive, Transactions - and reference and housekeeping on the right, pushed to the far edge. Overview, Info, Nodes, Settings and Exit are icons alone; Send, Receive and Transactions keep their labels, because those two in particular are not reliably distinguishable as pictures. The words survive in the tooltip and the status bar
- **Exit** on the toolbar, so leaving no longer means a trip through a menu. It asks first, and only mentions staking when a wallet is actually staking - the check spans every open wallet, since staking is a per-wallet timed unlock and a background wallet can be staking while another is on screen. Staying is what Return and Escape choose, and Exit is red. Both Exit actions and the tray share one confirmation, so it cannot be bypassed by route
- Its icon is a door with an arrow leaving through it, not a cross: a cross means "close this window", and this closes the application. Drawn with QPainter on a 64x64 grid rather than shipped as a pixmap, so it stays sharp at every size and takes the toolbar's colour - and it carries a second, white version for hover, because a stylesheet's `color` reaches a button's label and never its icon, which left the old glyph unreadable against the red
- **Settings** on the toolbar beside it. Both are separate actions from their menu counterparts: `QAction::QuitRole` and `PreferencesRole` relocate those into the application menu on macOS, so reusing them would have made the toolbar buttons vanish there

#### Fit and finish
- The dotted focus rectangle the platform stamps inside a focused button or tab is gone. It fought every other line in the interface and read as damage on a filled accent button. Focus is still shown, in the application's own language - an accent border, which is the same border the widget already has, so nothing moves when focus arrives. It stays on under the System theme, where no stylesheet of ours replaces it and it is the only indication a keyboard user gets
- The Settings dialog is themed like the rest of the application. The selected tab took a grey barely different from its neighbours and is now the accent outright; the two secondary buttons were stacked full-width above OK, making them look as important as it, and now sit on one row at the left with OK the only button that looks committal
- The Info page reads at a proper size, and splits into two columns when the window is wide enough - Project on the left, Community, Network and Development on the right - falling back to one column below 900px, where the long GitHub URLs would start to elide
- The Nodes page has a heading and a line saying what it is for; that row held nothing but the Refresh button, so the page opened on an unexplained pair of tables
- New button at the bottom of the Balances card: **Rescan wallet**, for a balance that reads too low. It runs the new `recoverwallet` RPC - a full scan of the chain that extends the address window as history is found, which is what an ordinary rescan cannot do. It explains what will happen before starting, asks for the wallet to be unlocked, runs off the GUI thread behind a progress dialog, and says how many passes it took. If the node is still catching up it explains why a rescan is not possible instead of scanning an incomplete chain

### Versioning
- Version numbers are now three components (`0.20.0`), not four; `getnetworkinfo` reports `200000` and `/Taler:0.20.0/`
- CI verifies that the built binary's version matches the tag before publishing a release

### Run Mode and API Sidecar in the Network Version
- The node now says whether it is the desktop wallet or a daemon, in the BIP-14 comment field of its user agent: `/Taler:0.20.0(GUI)/` or `/Taler:0.20.0(SERV)/`. **Note that this reverses a deliberate upstream choice** - Bitcoin reports the same agent for `bitcoind` and `bitcoin-qt` specifically so that attackers cannot pick out GUI users, who are more likely to be desktop machines holding a balance. It was adopted knowingly, for fleet visibility; `-uacomment` values still follow, unchanged and last
- An API sidecar running alongside the node can register itself and is then reported too: `/Taler:0.20.0(SERV; api:1.2.0)/`. New RPCs `sidecarregister`, `sidecarheartbeat`, `sidecarderegister` and `getsidecarinfo`. One sidecar at a time, guarded by a token issued at registration so a stray process with RPC credentials cannot displace or retire a live one; re-registering with that token is how a sidecar reports a new version after upgrading
- The registration lapses on its own if the heartbeat stops - 90 seconds by default, swept every 15 - so a sidecar that crashes does not leave the node advertising an API nobody is serving. It is not persisted: a node that restarts has no sidecar until one registers again, which it must do anyway
- The name and version a sidecar supplies are broadcast to other people's machines, so they are refused rather than cleaned if they contain anything that could be mistaken for structure. BIP-14's own comment set is not sufficient here: it permits `;`, which is the separator *between* comments, so a version of `1.0;GUI` would have passed a plain BIP-14 check and then split into two comments on every peer that parsed it. Letters, digits, `.`, `-` and `_` only
- Registration is refused, leaving the node exactly as it was, if the resulting agent would exceed the 256-byte limit - a truncated agent loses its closing `/` and every parser downstream reads the remains as part of the version
- `strSubVersion` was a bare global written once before the network threads started; it is now private behind `GetSubVersion()`/`SetSubVersion()` under a lock, because a sidecar can now change it while those threads are composing handshakes
- **A change reaches new peers only.** The agent is sent once per connection, during the version handshake, and there is no message in the protocol for revising it, so peers already connected keep what they were told until they reconnect. `getnetworkinfo` and `getsidecarinfo` always show the truth locally and immediately
- `getnetworkinfo` gained `runmode` and `sidecar`; the Nodes page gained Mode and API columns, parsed from each peer's agent and left blank for peers that report nothing. What a peer reports there is its own claim, not something this node verified

### Network
- Removed dead DNS seeds: `dnsseed.talercrypto.com`, `talerseed2.vovanchik.net`, `talerseed3.vovanchik.net` (mainnet and testnet)
- Removed two hardcoded fixed seeds that have been unreachable for over six months: `159.69.86.60` and `104.28.7.132`. The remaining fixed seeds, `128.140.124.27` and `178.124.162.209`, are unchanged

### Node Fixes
These were found by building and running the test suite, which had never been enabled in this fork:
- `CChain::FindEarliestAtLeast` could hang the node: its binary search set the low bound to the midpoint instead of the midpoint plus one, so it stopped making progress once the window narrowed to two blocks and spun forever. Reached by `importmulti` and `importprivkey` with a timestamp, by `pruneblockchain`, and by a restore with a birthday
- Phrase wallets derived addresses that did not match the published spec: the stored seed key *is* the BIP-32 master key, but it was passed through `MasterKeyFromSeed` a second time, hashing it into an unrelated master. The addresses were stable, so nothing looked wrong - they simply were not the addresses the same phrase produces anywhere else
- A phrase wallet's first address was not recoverable from its phrase: the HD upgrade path ran before wallet creation, gave the wallet a random legacy seed and left one key derived from it at the head of the keypool
- `-newwalletmnemonic` never fired for the default unnamed wallet: `WalletLocation("").Exists()` tests the wallet *directory*, which always exists, so the flag was skipped as "wallet already exists". Added `WalletLocation::HasWalletData()`
- Regtest could not start: it carried Bitcoin's genesis block and asserted Bitcoin's genesis hash, every difficulty limit sat below the target its own blocks use, and eleven staking and difficulty parameters were left at zero, so the first `generate()` divided by a zero-length averaging window
- Regtest could not be mined past a dozen blocks: `fPowNoRetargeting` was honoured only on the legacy retarget path, not by the difficulty algorithm in use, so instantly-minted blocks tripled the difficulty each time and `generate(30)` silently returned 24 blocks
- Regtest changes affect only that local test chain; mainnet and testnet consensus parameters are untouched

### Wallet Bookkeeping and Restore
- **Unconfirmed transactions are no longer deleted at start-up.** Every wallet load permanently erased - from wallet.dat, not merely abandoned - any transaction that was unconfirmed and more than an hour old. A payment that had simply not been mined yet vanished from the wallet's history at the next start, along with any record that the coins had been sent. Nothing about "unconfirmed for an hour" means dead: the fee may be low, the mempool may have dropped it, the node may have been off. Transactions that genuinely cannot come back are released at runtime, on evidence, and marked abandoned rather than deleted, so they stay visible as history. Deliberately discarding wallet history remains what `-zapwallettxes` is for
- New RPC `recoverwallet`: rescan the whole chain for a loaded wallet while extending the address look-ahead as history is found. An ordinary rescan only looks for addresses the wallet has already derived, so a wallet restored from an older backup - or one that was locked while it scanned - stays blind to everything it used after that point. Requires the wallet unlocked, and refuses a pruned chain or a syncing node rather than reporting a confident, wrong balance
- **The displayed balance could go stale and stay stale, most often while staking.** The overview polls the wallet with `tryGetBalances`, which returns the balance and the block height together under one lock so that the two describe the same instant. The poll then threw that height away and asked the node again, after the locks were released. When a block landed in that window the poll stored the *new* height beside the *old* balance, so every later poll saw the height it expected and skipped the update - and the figure on screen stayed wrong until some later block happened to win the race. Staking made it far more likely, because `CreateCoinStake` takes `cs_main` and `cs_wallet` every second, so the poll's try-lock fails often. What kept it from healing quickly is that the balance change with no transaction behind it is **coinbase maturity**: when a staking reward matures, Immature falls and Available rises purely because the height advanced, so the "a transaction changed, refresh anyway" safety net never fires. The poll now uses the height that came back with the balance, as upstream does. RPC `getbalance` was never affected - it computes on demand
- Restoring a wallet from a recovery phrase **with a passphrase** failed outright with "Unable to derive the look-ahead addresses". Encrypting leaves the wallet locked, a locked keypool cannot be topped up, and the look-ahead window is derived after encryption - so the common path through the Restore dialog could never work. The wallet is now unlocked for the scan and locked again afterwards, rather than encrypting after the scan, which would have written thousands of freshly derived private keys to disk in the clear first
- Default keypool raised from 250 to 1000, matching upstream. Every staked block consumes a key, so a wallet that stakes for a few months walks through a 250-key pool - and a backup restored afterwards can only find history as far as its pool reaches
- Bulk key derivation is roughly five times faster: deriving one BIP-44 key rebuilt the master from the phrase every time, including a full 2048-round PBKDF2, so a keypool top-up paid that cost per key. The account key is now derived once per run and wiped when the run ends. Creating a wallet with a 2000-key pool went from 8.0s to 1.6s
- Transactions in a block that loses a reorg go back to being unconfirmed, as upstream does, instead of being marked abandoned on the spot. Abandoning immediately freed the inputs of transactions that were usually mined again a block or two later
- Abandonment now goes through the wallet's own machinery, so it is written to disk, invalidates the cached balances, and follows descendants. The two places that set the flag directly did none of those things, so whatever they decided was lost at the next restart and the displayed balance did not move
- The sweep that releases stuck transactions now requires two independent proofs rather than a clock: the transaction's block must be one we know that is provably off the active chain, and the transaction must be one that can only ever exist in that block - a coinbase, or the coinstake of a proof-of-stake block. Previously any unconfirmed transaction older than fifteen minutes was abandoned, which included ordinary payments that were merely waiting
- `CWallet::AbandonTransaction` no longer aborts the node when it meets an inconsistent wallet; it skips the transaction and logs. The assert was tolerable when only the `abandontransaction` RPC could reach it, but that code now runs unattended on every staking node

### Test Infrastructure
- The inherited test suite had never been built: `configure.ac` defaults `use_tests=no` in this fork, so no build or CI job had ever compiled a test and the code had drifted away from the tests unnoticed
- `test_bitcoin.cpp` and several suites ported to this fork's APIs: `CheckProofOfWork(header, height, params)`, `CBlockIndex::nChainWork()`, `CWallet::GetBalance().Immature`, `GetDifficulty(isPoS, index)`, `CFeeRate::ToString()`
- `skiplist_tests` rewritten against block times, since this chain has no `nTimeMax`; it is now the regression test for the `FindEarliestAtLeast` hang
- The Qt test binary had never been built either: it still compiled BIP70 payment-request tests for a payment server this fork does not have, expected Bitcoin's `bitcoin:` URI scheme rather than `taler:`, and asserted Bitcoin's regtest merkle root. It now builds and passes, and the Linux CI build runs it offscreen
- New Qt suite `rewardsummarytests` covers the home screen's reward arithmetic: month bucketing and its edges, rewards counted but payments and coinstake principal not, last-seen dates, the day thresholds at 6/7/30/31, and staking readiness
- New unit suites `bip39_tests` and `bip44_tests` check the wordlist hash, the official BIP-39 vectors, and every address in the shared Taler vectors for mainnet and regtest
- New functional suites: `wallet_mnemonic.py` (spec vectors, restore, encryption, restore with a passphrase, rejections, imported keys, funds recovered from address index 150) and `wallet_mnemonic_startup.py` (the headless flag, file permissions, overwrite refusal, phrase absent from the log)
- New functional suite `rpc_sidecar.py` covers the sidecar registry end to end: registration and its effect on the user agent, heartbeat, version update through the token, the token guarding heartbeat and deregistration, idempotent deregistration, every refusal, and the automatic expiry of a sidecar that stops answering. It caught the `;` hole in the original sanitisation on its first run
- New functional suite `wallet_recover.py` covers the balance that reads too low: a wallet holding funds it has never scanned for reports zero, then reports the full amount after `recoverwallet`. It also checks that a locked wallet is refused rather than scanned pointlessly
- New Qt suite `splashtests` checks that the start-up artwork is present *and that this build can decode it* - the JPEG plugin failing to link produces no error, just a null image and a plain panel, which is easy to ship without noticing. It caught exactly that twice while the feature was being built
- New functional suite `wallet_stake_reorg.py`, the first test in this project to stake a real proof-of-stake block. Reaching that took working out that the kernel needs the *chain's* block times to span more than `GetStakeModifierSelectionInterval()` - about 38 minutes on regtest - so blocks must be mined with the clock stepped forward between them; advancing mocktime alone never produces a stake. It then reorgs the stake block away and asserts that the staked coin is never lost by either route, that the reported balance keeps agreeing with the wallet's own UTXOs throughout, and that an ordinary payment sharing the orphaned block is left alone
- New upgrade gate `test/compat/wallet_compat.py` drives a real build of the previous release: a 0.19 wallet through 0.20 and back to 0.19 with seed, addresses, balance and file location intact, and a phrase wallet refused by 0.19 without damage. It runs on mainnet parameters with networking off, because 0.19 cannot start regtest at all
- The functional test framework was writing `bitcoin.conf`, which the node never reads, and looked for `bitcoind`/`bitcoin-cli`; every functional test had been silently starting an unconfigured mainnet node
- `policyestimator_tests`, `pow_tests`, `txindex_tests` and `versionbits_tests` are no longer built - they test subsystems this fork does not have
- 55 unit suites, 30 Qt tests and four functional suites pass; 13 inherited suites still carry Bitcoin's keys, addresses or subsidy schedule and are listed with reasons in `test/unit-test-suites.txt`
- New `.github/workflows/test.yml` runs the unit suites, the functional suites and a check that the vendored derivation vectors still match the published spec on every push, plus the upgrade gate before a release is tagged

### Documentation
- README opens with the Taler artwork, stored as a 274KB JPEG rather than the 1.5MB source PNG - it is a photographic gradient, so PNG is the wrong format, and every visitor to the repository downloads it. Full resolution, displayed at 900px, so it stays sharp on a retina screen; the alt text spells out the four claims and both URLs for anyone reading without images
- Release notes for 0.20.0 in `doc/release-notes-0.20.0.md`

---

# Taler 0.19.6.8

## Release Date
April 2026

## Major Changes

### Staking UI
- Added "Start staking" panel to the Overview page with duration selector (1h / 6h / 24h / 7d / 30d)
- Live countdown timer showing remaining staking time with progress bar
- "Stop staking" button with confirmation dialog
- Passphrase prompt via existing wallet unlock dialog (new UnlockStaking mode)
- Auto-relock via QTimer when staking duration expires
- Staking panel hidden for unencrypted wallets (staking is always-on without encryption)
- Panel appears automatically after encrypting the wallet (no restart needed)
- Non-persistent: staking state resets on app restart (wallet starts locked)
- Full translations for all 34 supported languages

### Icon Theme Fix
- All icons now render in theme-adaptive color on both light and dark themes
- Enabled icon colorization on macOS and Windows (previously only Linux)
- Icons use WindowText palette color, matching the rest of the UI text

### Static Linking for macOS Distribution
- Switched macOS release builds from Homebrew dynamic libraries to fully static depends/ system
- Release binaries no longer require Homebrew packages on user machines
- Eliminates "dyld: Library not loaded" crashes for boost@1.85 and other libraries

### Dependency Upgrades (depends/ system)
- Boost: 1.64.0 → 1.88.0 (ARM64 macOS support, C++17)
- OpenSSL: 1.0.1k → 3.4.1 (ARM64 macOS support, security fixes, modern TLS)
- Qt: 5.9.6 → 5.15.16 (ARM64 macOS support, last Qt5 LTS)
- libevent: 2.1.8 → 2.1.12
- ZeroMQ: 4.3.1 → 4.3.5
- protobuf: 2.6.1 → 3.21.12
- qrencode: 3.4.4 → 4.1.1
- zlib: 1.2.11 → 1.3.1
- miniupnpc: 2.0.20180203 → 2.2.8
- macOS minimum version: 10.10 → 11.0 (required for Apple Silicon)

### Build System
- macOS CI workflow rewritten to use depends/ static build (matches Windows CI)
- build_macos.sh rewritten to use identical depends/ flow as CI
- Only build tools (automake, libtool, pkg-config) needed from Homebrew
- otool -L verification step in CI to catch dynamic linking regressions
- Fixed dead Boost download URL (dl.bintray.com → archives.boost.io)
- Replaced deprecated SSL_library_init() with OPENSSL_init_ssl() for OpenSSL 3.x
- Fixed PATH word-splitting in depends/funcs.mk when user PATH contains spaces (e.g., VMware Fusion)
- Added -isysroot to build_darwin_CC/CXX for macOS 15/26 SDKs
- Updated Qt 5.15 patches (fix_qt_pkgconfig, fix_no_printer) for 5.15 source layout
- Disabled OpenGL and Vulkan in Qt for macOS (AGL framework removed in macOS 26 SDK)
- Patched Qt's bundled libpng to skip Classic Mac OS fp.h include (TARGET_OS_MAC clash)
- Removed obsolete Qt configure flags: -no-qml-debug, -no-xinput2 (gone in Qt 5.15)
- Made Qt5CglSupport an optional pkg-config dependency (not built without OpenGL)
- Updated miniupnpc build/stage paths for 2.2.8 layout (build/libminiupnpc.a, include/)
- Updated UPNP_GetValidIGD call in net.cpp for miniupnpc API 18 (7-arg signature)
- Fixed build_linux.sh tool check (libtool → libtoolize, matches Debian/Ubuntu package layout)
- Added missing <array> include in net_processing.cpp and qt/sendcoinsdialog.cpp, <stdexcept> in support/lockedpool.cpp (required by stricter modern GCC)
- Replaced raw-function-pointer signals2 disconnect with connection object in init.cpp (Ubuntu 24.04 Boost 1.83+)
- Bumped Linux x64/ARM64 CI runners from ubuntu-22.04 to ubuntu-24.04
- Linux CI and build_linux.sh now statically link Boost (libboost_*.a) so binaries run on any Ubuntu regardless of installed libboost version
- All build workflows (Linux x64/ARM64, macOS, Windows) now also trigger on pull_request against main for compile-only verification; archive/upload/release steps remain gated on tag pushes
- Bumped actions/checkout from v4 to v5 across all workflows to silence the Node.js 20 deprecation warning
- build_linux.sh now pre-checks system libs via pkg-config (Qt5Core/Gui/Network/Widgets, openssl, libevent, libzmq, protobuf, libqrencode) plus boost/version.hpp, and points to --install-deps if missing
- Added build_windows.sh (cross-compile from Ubuntu via MinGW-w64), matching the Windows CI workflow step-for-step for local debugging
- build_linux.sh and build_windows.sh: moved --install-deps handling ahead of the tool-availability check so it works on a fresh machine
- depends/packages/openssl.mk: pass WINDRES=$(host)-windres for mingw32 so OpenSSL 3.4.1 finds the prefixed MinGW resource compiler on Ubuntu
- depends/packages/qt.mk: added -no-feature-schannel for mingw32 (schannel wins by default on Windows and blocks -openssl-linked) and sed-patch qtbase/src/network/configure.json during preprocess to make the MinGW OpenSSL link test use OpenSSL 3.x lib names (-lssl -lcrypto) plus Windows system libs (-lws2_32 -lgdi32 -lcrypt32); Qt 5.15's four built-in openssl sources otherwise all fail on OpenSSL 3.x+MinGW
- depends/packages/qt.mk: replaced -dbus-runtime with -no-dbus globally so Qt doesn't emit link-time dependencies on libQt5DBus.a from Qt5ServiceSupport/Qt5ThemeSupport/xdgdesktopportal; taler-qt doesn't use D-Bus on any platform, and depends/ targets (macOS, Windows) don't need it — Linux builds use apt Qt and are unaffected
- Added retry-once to the depends/ build step in build_macos.sh, build_windows.sh and both CI workflows to work around an intermittent Qt 5.15 moc/plugin parallel-build race
- build-aux/m4/bitcoin_qt.m4: link CoreVideo, IOSurface, Carbon, QuartzCore and Metal frameworks on darwin so Qt's static libqcocoa.a resolves CVDisplayLink*, IOSurface*, Carbon keyboard, CAMetalLayer/CAShapeLayer symbols on macOS 26 SDK
- configure.ac: AC_CHECK_LIB bcrypt on Windows so Boost 1.88 filesystem's unique_path() BCrypt-based random generator links
- src/wallet/db.cpp: dropped the __MINGW32__-branch using the removed Boost 1.85+ copy_option API; fs::copy_options::overwrite_existing works uniformly on Boost 1.68+ now that all platforms use modern Boost
- build_macos.sh: added clean (keeps depends/ prefix) and clean-all (wipes everything) subcommands; always re-runs autogen.sh when configure.ac, any build-aux/m4/*.m4, or any Makefile.am is newer than configure so M4 edits take effect without manual steps
- Rewrote README.md as a modern open-source project landing page: CI/metadata badges, SEO-oriented description, Docker and docker compose quick-start, cross-platform self-compile pointers, explorer and community links
- build-aux/m4/bitcoin_qt.m4: link Windows system libs (wtsapi32, userenv, netapi32) and Qt's Qt5WindowsUIAutomationSupport static library before the -lqwindows static-plugin link test; without these four additions, Qt 5.15's libqwindows.a emitted undefined references (WTS*, NetShare*, GetUserProfileDirectoryW, QWindowsUiaWrapper::*), the link test silently failed, and taler-qt.exe wasn't built. Mirrors the darwin framework fix for libqcocoa.a shipped earlier.
- build_windows.sh: auto-regenerate configure when configure.ac, any build-aux/m4/*.m4, or any Makefile.am is newer than configure (matches build_macos.sh); avoids stale-configure silent-failure footguns when M4 files change.
- build-aux/m4/bitcoin_qt.m4: moved the per-platform library additions (Windows system libs + Qt5WindowsUIAutomationSupport on Windows, Cocoa frameworks on darwin) to run *before* the QMinimalIntegrationPlugin link test rather than after it. The qminimal plugin transitively pulls in Qt5Core/Qt5Gui, which reference those platform symbols on Windows; with additions running after qminimal, its link silently failed and the GUI was disabled before the qwindows check even ran.

### Belarusian Translation Fix
- Standardized wallet terminology: "кашалёк" → "гаманец" across all inflections

---

# Taler 0.19.2.8

## Release Date
March 2026

## Major Changes

### Build Fixes
- Fixed Windows cross-compilation OpenSSL compatibility (TLS_client_method not available in OpenSSL 1.0.x)
- Added compile-time version check to use SSLv23_client_method on OpenSSL 1.0.x and TLS_client_method on 1.1.0+

### UI Improvements
- Added Nodes page showing hardcoded seeds, community seeds, and discovered peers
- Added connectivity checker with background TCP probing and color-coded status
- Added community seed nodes fetched from GitHub (taler-seeds repository)
- Added About/Info page with project links, explorers, community, and development resources
- Changed Nodes tab icon from info to network connection icon
- Filled all missing translations across 33 language files (744 strings total)

### Network Enhancements
- Added remote seed fetching from GitHub taler-seeds repository

---

# Taler 0.18.44.7

## Release Date
November 2025

## Previous Changes

### Windows Build Fixes
- Fixed gmtime_r in wallet/init.cpp (same MinGW issue as utiltime.cpp)
- Fixed Boost filesystem copy_options API incompatibility in wallet/db.cpp
- MinGW uses Boost v2 API: `fs::copy_option::overwrite_if_exists`
- POSIX uses Boost v3 API: `fs::copy_options::overwrite_existing`
- Fixed gmtime_r issue for MinGW cross-compilation in utiltime.cpp
- Added __MINGW32__ and __MINGW64__ checks to use gmtime_s on Windows
- Previously only checked _MSC_VER which doesn't cover MinGW
- Fixes "gmtime_r was not declared in this scope" compilation error
- Fixed BDB 18 MinGW patch to target correct file (mut_win32.c instead of atomic.h)
- Patch now applies cleanly by adding macros directly to mut_win32.c
- Defines WINCE_ATOMIC_MAGIC, interlocked_val, and atomic_read for non-WinCE builds
- Fixed BDB 18 compilation for Windows MinGW cross-compilation
- Added fix_mingw_atomics.patch to define missing WINCE_ATOMIC_MAGIC and interlocked_val macros
- Resolves "implicit declaration of function 'WINCE_ATOMIC_MAGIC'" error
- Resolves "interlocked_val undeclared" error in mut_win32.c

### Build Fixes
- Fixed BDB 18 installation script to skip missing documentation files
- Changed from `make install` to `make install_setup install_include install_lib`
- Avoids error: "cannot stat 'bdb-sql': No such file or directory"
- Fixed Qt 5.9.6 patch application in depends build system (GCC 11+ compatibility)
- Added Qt 5.9.6 patch for GCC 11+ compatibility (missing <limits> header)
- Fixed executable permissions on depends/config.guess and depends/config.sub for Windows builds
- Fixed function ordering in scrypt.cpp for Linux x64 SSE2 compilation
- Fixed missing `<deque>` header in httpserver.cpp for Linux builds
- Added Xvfb for headless Qt GUI testing in Linux CI/CD
- Fixed GitHub Actions macOS build compatibility
- Removed macOS version-specific endian header dependency in scrypt.cpp
- Now uses portable endian implementations across all platforms
- Added explicit --with-gui flag to ensure Qt GUI is built
- Made taler-qt binary copy conditional for build flexibility
- Added protobuf to required dependencies for Qt GUI support
- Fixed BDB 18.1.40 patch for Windows (corrected line numbers for win_db.h)
- Fixed Dockerfile to match working GitHub Actions Linux build configuration
- Optimized .dockerignore to include necessary build files
- Removed Travis CI configuration (replaced by GitHub Actions)
- Fixed trailing whitespace in src/Makefile.am causing automake errors
- Fixed Berkeley DB 18.1.40 case-sensitive include issue for Windows cross-compilation (WinIoCtl.h → winioctl.h)

### Berkeley DB Standardization
- All platforms now use Berkeley DB 18.1.40 for wallet compatibility
- Added contrib/install_db18.sh script to build BDB 18 from source
- Docker now builds BDB 18 from source and links statically
- GitHub Actions Linux builds (x64 and ARM64) now build BDB 18 from source
- macOS already uses BDB 18 from Homebrew
- Ensures wallet files are compatible across all platforms
- Replaced system libdb5.3++ with custom-built BDB 18.1.40


### Docker Fixes
- Fixed missing runtime dependencies (libzmq.so.5, libqrencode)
- Added libzmq5 and libqrencode4 to runtime stage dependencies
- Binaries now have all required shared libraries to run
- Fixed entrypoint.sh permission denied error
- Root cause: Entrypoint script was copied from wrong stage (runtime instead of builder)
- Solution: Copy entrypoint.sh from builder stage where git repo was cloned
- Changed `COPY docker/entrypoint.sh` to `COPY --from=builder /taler/docker/entrypoint.sh`
- Complete redesign: Docker now clones from git repository instead of copying local files
- Eliminated all .dockerignore complexity and cross-platform build artifact issues
- Docker automatically finds and checks out latest git tag
- Docker build now gets exact same source tree as GitHub Actions
- Solution: `git clone https://github.com/abkvme/taler.git . && git checkout $(git describe --tags $(git rev-list --tags --max-count=1))`
- No more macOS ARM64 artifacts contaminating Linux builds
- Fixed Docker build failing with "cannot find univalue/.libs/libunivalue.a"
- Root cause: Overly broad .dockerignore patterns were excluding source files needed for build
- Pattern `Makefile` matched ALL Makefiles including source `Makefile.am` files
- Solution: Use leading `/` for root-only patterns (e.g., `/Makefile` instead of `Makefile`)
- Now correctly excludes ONLY generated files while keeping source files
- Binary artifacts (*.o, *.a, *.la) still excluded everywhere to prevent cross-platform conflicts
- Fixed UniValue linking errors by excluding build artifacts from Docker context
- Root cause: COPY . . was copying macOS ARM64 compiled objects (.o, .a, .la files)
- Added comprehensive build artifact exclusions to .dockerignore
- Docker now gets clean source tree like GitHub Actions checkout
- Excludes: *.o, *.a, *.la, .libs/, config.status, Makefile, libtool, etc.
- Fixed Qt moc compilation error by excluding platform-specific generated files from Docker
- Added src/qt/moc_*.cpp and src/qt/*.moc to .dockerignore
- Root cause: macOS-generated moc files were being copied to Linux container
- Docker now regenerates moc files for target platform, matching GitHub Actions behavior
- Fixed Dockerfile to match GitHub Actions Linux build exactly
- Build includes Qt libraries (same as GitHub Actions) but only daemon binaries are shipped in container
- Removed .git/ from .dockerignore to ensure correct source tree is used
- Configure flags: --with-incompatible-bdb --with-gui CXXFLAGS="-O2"
- Fixed Dockerfile build by removing manual univalue compilation
- Root cause: Pre-building univalue caused autoconf to treat it as external library instead of embedded
- Now uses same build process as GitHub Actions (autogen.sh → configure → make)
- Removed Qt/GUI dependencies from Docker build (daemon-only)

### Docker Support
- Added Dockerfile with multi-stage build for optimized container size
- Added docker-compose.yml for easy node deployment
- Support for both amd64 and arm64 architectures
- Daemon-only build (no GUI) for containers
- Comprehensive Docker documentation in README-DOCKER.md
- Example configuration file for Docker environments
- Automated entrypoint script with configuration management

### CI/CD Enhancements
- Added GitHub Actions workflows for all major platforms
- Automated multi-platform binary releases (macOS ARM64, Linux x64, Linux ARM64, Windows x64)
- All platforms now use Berkeley DB 18.x with --with-incompatible-bdb flag
- Windows builds use MinGW cross-compilation with depends system

### Branding Update
- Updated to new circular Taler logo with transparent background
- New app icons across all platforms (macOS ICNS, Windows ICO, PNG)
- Updated splash screen and about dialog icons

### CI/CD Automation
- Added GitHub Actions workflow for automated macOS builds
- Automatic binary packaging and release creation on tag push
- 1-year artifact retention for release binaries


### macOS Support (Apple Silicon & Intel)
- Added full support for macOS ARM64 (M1/M2/M3) and Intel architectures
- New automated build script (`build_macos.sh`) for easy compilation on macOS
- Fixed Qt High DPI initialization bug that caused GUI hangs

### Berkeley DB Upgrade
- Upgraded from Berkeley DB 4.8 to 18.1.40
- Required for ARM64 compatibility (BDB 4.8 lacks ARM64 mutex support)
- Existing wallets remain compatible with `--with-incompatible-bdb` flag

### Dependency Updates
- Boost 1.85 compatibility fixes
- Updated Boost filesystem API calls
- Qt 5 compatibility improvements for modern macOS SDKs

### Architecture Improvements
- ARM64-compatible cryptographic implementations (Scrypt, Lyra2Z)
- Platform-specific optimizations (SSE2 on x86/x64, generic on ARM64)
- Fixed endian function handling for cross-platform compatibility

## Platform Support
- ✅ macOS ARM64 (Apple Silicon)
- ✅ macOS Intel (x86_64)
- ✅ Linux x86_64
- ✅ Linux ARM64
- ✅ Windows x64

## Building on macOS

### Prerequisites
Install build tools via Homebrew:
```bash
brew install automake libtool pkg-config
```

All library dependencies (Boost, OpenSSL, Qt, etc.) are built automatically from source as static libraries by the depends/ system.

### Build
```bash
chmod +x build_macos.sh
./build_macos.sh
```

First run takes 15-30 minutes (building dependencies). Subsequent builds reuse cached dependencies. Binaries will be in `./bin/` directory.

## Migration Notes
- Backup your wallet before upgrading
- BDB upgrade is transparent with `--with-incompatible-bdb` flag
- No changes to runtime configuration or data directories
