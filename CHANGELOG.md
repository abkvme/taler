# Taler 0.18.20.7

## Release Date
November 2025

## Major Changes

### Docker Fixes
- Fixed Docker build failing with "cannot find univalue/.libs/libunivalue.a"
- Root cause: Overly broad .dockerignore patterns were excluding source files needed for build
- Pattern `Makefile` matched ALL Makefiles including source `Makefile.am` files
- Solution: Use leading `/` for root-only patterns (e.g., `/Makefile` instead of `Makefile`)
- Now correctly excludes ONLY generated files while keeping source files
- Binary artifacts (*.o, *.a, *.la) still excluded everywhere to prevent cross-platform conflicts

## Previous Releases

# Taler 0.18.19.7

## Release Date
November 2025

## Major Changes

### Docker Fixes
- Fixed UniValue linking errors by excluding build artifacts from Docker context
- Root cause: COPY . . was copying macOS ARM64 compiled objects (.o, .a, .la files)
- Added comprehensive build artifact exclusions to .dockerignore
- Docker now gets clean source tree like GitHub Actions checkout
- Excludes: *.o, *.a, *.la, .libs/, config.status, Makefile, libtool, etc.

## Previous Releases

# Taler 0.18.18.7

## Release Date
November 2025

## Major Changes

### Docker Fixes
- Fixed Qt moc compilation error by excluding platform-specific generated files from Docker
- Added src/qt/moc_*.cpp and src/qt/*.moc to .dockerignore
- Root cause: macOS-generated moc files were being copied to Linux container
- Docker now regenerates moc files for target platform, matching GitHub Actions behavior

## Previous Releases

# Taler 0.18.17.7

## Release Date
November 2025

## Major Changes

### Docker Fixes
- Fixed Dockerfile to match GitHub Actions Linux build exactly
- Build includes Qt libraries (same as GitHub Actions) but only daemon binaries are shipped in container
- Removed .git/ from .dockerignore to ensure correct source tree is used
- Configure flags: --with-incompatible-bdb --with-gui CXXFLAGS="-O2"

## Previous Releases

# Taler 0.18.16.7

## Release Date
November 2025

## Major Changes

### Docker Fixes
- Fixed Dockerfile build by removing manual univalue compilation
- Root cause: Pre-building univalue caused autoconf to treat it as external library instead of embedded
- Now uses same build process as GitHub Actions (autogen.sh → configure → make)
- Removed Qt/GUI dependencies from Docker build (daemon-only)

## Previous Releases

# Taler 0.18.15.7

## Release Date
November 2025

## Major Changes

### Build Fixes
- Fixed BDB 18.1.40 patch for Windows (corrected line numbers for win_db.h)
- Fixed Dockerfile to match working GitHub Actions Linux build configuration
- Optimized .dockerignore to include necessary build files
- Removed Travis CI configuration (replaced by GitHub Actions)
- Fixed trailing whitespace in src/Makefile.am causing automake errors

## Previous Releases

# Taler 0.18.14.7

## Release Date
November 2025

## Major Changes

### Docker Support
- Added Dockerfile with multi-stage build for optimized container size
- Added docker-compose.yml for easy node deployment
- Support for both amd64 and arm64 architectures
- Daemon-only build (no GUI) for containers
- Comprehensive Docker documentation in README-DOCKER.md
- Example configuration file for Docker environments
- Automated entrypoint script with configuration management

## Previous Releases

# Taler 0.18.13.7

## Release Date
November 2025

## Major Changes

### Build Fixes
- Fixed Berkeley DB 18.1.40 case-sensitive include issue for Windows cross-compilation (WinIoCtl.h → winioctl.h)

## Previous Releases

# Taler 0.18.12.7

## Release Date
November 2025

## Major Changes

### CI/CD Enhancements
- Added GitHub Actions workflows for all major platforms
- Automated multi-platform binary releases (macOS ARM64, Linux x64, Linux ARM64, Windows x64)
- All platforms now use Berkeley DB 18.x with --with-incompatible-bdb flag
- Windows builds use MinGW cross-compilation with depends system

### Build Fixes
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

## Previous Releases

# Taler 0.18.2.7

## Release Date
November 2025

## Major Changes

### Branding Update
- Updated to new circular Taler logo with transparent background
- New app icons across all platforms (macOS ICNS, Windows ICO, PNG)
- Updated splash screen and about dialog icons

### CI/CD Automation
- Added GitHub Actions workflow for automated macOS builds
- Automatic binary packaging and release creation on tag push
- 1-year artifact retention for release binaries

## Previous Releases

# Taler 0.18.0.7

## Release Date
November 2025

## Major Changes

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
Install dependencies via Homebrew:
```bash
brew install automake libtool pkg-config berkeley-db boost@1.85 \
             openssl qt@5 libevent qrencode zeromq
```

### Build
```bash
chmod +x build_macos.sh
./build_macos.sh
```

Binaries will be in `./bin/` directory.

## Migration Notes
- Backup your wallet before upgrading
- BDB upgrade is transparent with `--with-incompatible-bdb` flag
- No changes to runtime configuration or data directories
