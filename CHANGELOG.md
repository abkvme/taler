# Taler 0.18.11.7

## Release Date
November 2025

## Major Changes

### CI/CD Enhancements
- Added GitHub Actions workflows for all major platforms
- Automated multi-platform binary releases (macOS ARM64, Linux x64, Linux ARM64, Windows x64)
- All platforms now use Berkeley DB 18.x with --with-incompatible-bdb flag
- Windows builds use MinGW cross-compilation with depends system

### Build Fixes
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
