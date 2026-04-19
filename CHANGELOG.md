# Taler 0.19.5.8

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
