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

### Desktop Wallet Interface
- Theme selector in Settings: follow the system, light, or dark, applied without a restart; both themes are built from one stylesheet template and two colour sets so they cannot drift apart
- One card style across the whole application - same border, radius and surface for every panel, table and group box
- Available balance shown in an accent badge above the Balances card, to two decimals; the exact figure stays on the rows below
- Recovery phrase shown as a numbered 24-word grid, with a verification step before the wallet is created
- Overview and transaction views gained empty states, larger date and amount columns, and accent-coloured transaction icons
- Toolbar icons use the brand accent colour rather than a palette colour sampled once at startup, which made them invisible after a theme switch
- About page links to `explorer.taler.tech`
- Translations for the new interface in Russian, Ukrainian and Belarusian

### Versioning
- Version numbers are now three components (`0.20.0`), not four; `getnetworkinfo` reports `200000` and `/Taler:0.20.0/`
- CI verifies that the built binary's version matches the tag before publishing a release

### Network
- Removed dead DNS seeds: `dnsseed.talercrypto.com`, `talerseed2.vovanchik.net`, `talerseed3.vovanchik.net` (mainnet and testnet)

### Node Fixes
These were found by building and running the test suite, which had never been enabled in this fork:
- `CChain::FindEarliestAtLeast` could hang the node: its binary search set the low bound to the midpoint instead of the midpoint plus one, so it stopped making progress once the window narrowed to two blocks and spun forever. Reached by `importmulti` and `importprivkey` with a timestamp, by `pruneblockchain`, and by a restore with a birthday
- Phrase wallets derived addresses that did not match the published spec: the stored seed key *is* the BIP-32 master key, but it was passed through `MasterKeyFromSeed` a second time, hashing it into an unrelated master. The addresses were stable, so nothing looked wrong - they simply were not the addresses the same phrase produces anywhere else
- A phrase wallet's first address was not recoverable from its phrase: the HD upgrade path ran before wallet creation, gave the wallet a random legacy seed and left one key derived from it at the head of the keypool
- `-newwalletmnemonic` never fired for the default unnamed wallet: `WalletLocation("").Exists()` tests the wallet *directory*, which always exists, so the flag was skipped as "wallet already exists". Added `WalletLocation::HasWalletData()`
- Regtest could not start: it carried Bitcoin's genesis block and asserted Bitcoin's genesis hash, every difficulty limit sat below the target its own blocks use, and eleven staking and difficulty parameters were left at zero, so the first `generate()` divided by a zero-length averaging window
- Regtest could not be mined past a dozen blocks: `fPowNoRetargeting` was honoured only on the legacy retarget path, not by the difficulty algorithm in use, so instantly-minted blocks tripled the difficulty each time and `generate(30)` silently returned 24 blocks
- Regtest changes affect only that local test chain; mainnet and testnet consensus parameters are untouched

### Test Infrastructure
- The inherited test suite had never been built: `configure.ac` defaults `use_tests=no` in this fork, so no build or CI job had ever compiled a test and the code had drifted away from the tests unnoticed
- `test_bitcoin.cpp` and several suites ported to this fork's APIs: `CheckProofOfWork(header, height, params)`, `CBlockIndex::nChainWork()`, `CWallet::GetBalance().Immature`, `GetDifficulty(isPoS, index)`, `CFeeRate::ToString()`
- `skiplist_tests` rewritten against block times, since this chain has no `nTimeMax`; it is now the regression test for the `FindEarliestAtLeast` hang
- New unit suites `bip39_tests` and `bip44_tests` check the wordlist hash, the official BIP-39 vectors, and every address in the shared Taler vectors for mainnet and regtest
- New functional suites: `wallet_mnemonic.py` (spec vectors, restore, encryption, rejections, imported keys, funds recovered from address index 150) and `wallet_mnemonic_startup.py` (the headless flag, file permissions, overwrite refusal, phrase absent from the log)
- New upgrade gate `test/compat/wallet_compat.py` drives a real build of the previous release: a 0.19 wallet through 0.20 and back to 0.19 with seed, addresses, balance and file location intact, and a phrase wallet refused by 0.19 without damage. It runs on mainnet parameters with networking off, because 0.19 cannot start regtest at all
- The functional test framework was writing `bitcoin.conf`, which the node never reads, and looked for `bitcoind`/`bitcoin-cli`; every functional test had been silently starting an unconfigured mainnet node
- `policyestimator_tests`, `pow_tests`, `txindex_tests` and `versionbits_tests` are no longer built - they test subsystems this fork does not have
- 55 unit suites and both functional suites pass; 13 inherited suites still carry Bitcoin's keys, addresses or subsidy schedule and are listed with reasons in `test/unit-test-suites.txt`
- New `.github/workflows/test.yml` runs the unit suites, the functional suites and a check that the vendored derivation vectors still match the published spec on every push, plus the upgrade gate before a release is tagged

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
