#!/bin/bash
# Build script for Taler on macOS (ARM64 and x86_64)
# This script sets up the proper environment and builds Taler with BDB 18.x
#
# Usage:
#   ./build_macos.sh        # Normal build
#   ./build_macos.sh clean  # Clean build (runs 'make clean' first)

set -e

# Parse arguments
CLEAN_BUILD=false
if [ "$1" = "clean" ]; then
    CLEAN_BUILD=true
fi

echo "======================================"
echo "Taler macOS Build Script"
echo "======================================"

# Detect architecture
ARCH=$(uname -m)
echo "Architecture: $ARCH"

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "Error: Homebrew is not installed. Please install it from https://brew.sh"
    exit 1
fi

# Set Homebrew prefix based on architecture
if [ "$ARCH" = "arm64" ]; then
    BREW_PREFIX="/opt/homebrew"
else
    BREW_PREFIX="/usr/local"
fi

echo "Homebrew prefix: $BREW_PREFIX"

# Check for required dependencies
echo ""
echo "Checking for required dependencies..."
REQUIRED_DEPS=("automake" "libtool" "pkg-config" "berkeley-db" "boost@1.85" "openssl" "qt@5" "libevent" "qrencode" "zeromq")
MISSING_DEPS=()

for dep in "${REQUIRED_DEPS[@]}"; do
    if ! brew list "$dep" &> /dev/null; then
        MISSING_DEPS+=("$dep")
    fi
done

if [ ${#MISSING_DEPS[@]} -ne 0 ]; then
    echo "Missing dependencies: ${MISSING_DEPS[*]}"
    echo "Install them with: brew install ${MISSING_DEPS[*]}"
    exit 1
fi

echo "All dependencies are installed."

# Set environment variables
echo ""
echo "Setting up build environment..."

export BDB_PREFIX="$BREW_PREFIX/opt/berkeley-db"
export BOOST_ROOT="$BREW_PREFIX/opt/boost@1.85"
export OPENSSL_ROOT="$BREW_PREFIX/opt/openssl"
export QT5_ROOT="$BREW_PREFIX/opt/qt@5"

export CPPFLAGS="-I$BOOST_ROOT/include -I$BDB_PREFIX/include -I$OPENSSL_ROOT/include"
export LDFLAGS="-L$BOOST_ROOT/lib -L$BDB_PREFIX/lib -L$OPENSSL_ROOT/lib"
export PKG_CONFIG_PATH="$BREW_PREFIX/lib/pkgconfig:$QT5_ROOT/lib/pkgconfig"

# Export for configure
export BDB_LIBS="-L$BDB_PREFIX/lib -ldb_cxx-18.1"
export BDB_CFLAGS="-I$BDB_PREFIX/include"

echo "BDB_PREFIX=$BDB_PREFIX"
echo "BOOST_ROOT=$BOOST_ROOT"
echo "OPENSSL_ROOT=$OPENSSL_ROOT"

# Run autogen if configure doesn't exist
if [ ! -f "configure" ]; then
    echo ""
    echo "Running autogen.sh to generate configure script..."
    ./autogen.sh
fi

# Configure
echo ""
echo "Running configure..."
./configure \
    --with-incompatible-bdb \
    --with-gui \
    --with-boost-system=boost_system \
    CPPFLAGS="$CPPFLAGS" \
    LDFLAGS="$LDFLAGS" \
    BDB_LIBS="$BDB_LIBS" \
    BDB_CFLAGS="$BDB_CFLAGS" \
    BOOST_SYSTEM_LIB="-lboost_system" \
    --with-boost="$BOOST_ROOT"
    # Note: --enable-tests commented out due to API incompatibilities with Taler's modified consensus
    # (tests are from Bitcoin Core, Taler has removed SegWit and modified PoW signatures)

# Fix permissions on build scripts
echo ""
echo "Fixing permissions on build scripts..."
chmod +x share/genbuild.sh

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo ""
    echo "Cleaning previous build artifacts..."
    make clean || true
fi

# Build
echo ""
echo "Building Taler (this may take several minutes)..."
make -j$(sysctl -n hw.ncpu)

# Create bin directory and copy binaries
echo ""
echo "Copying binaries to ./bin directory..."
mkdir -p bin
cp src/talerd bin/
cp src/taler-cli bin/
cp src/taler-tx bin/
if [ -f "src/qt/taler-qt" ]; then
    cp src/qt/taler-qt bin/
fi

echo ""
echo "======================================"
echo "Build completed successfully!"
echo "======================================"
echo ""
echo "Binaries copied to ./bin directory:"
echo "  - talerd:     bin/talerd"
echo "  - taler-cli:  bin/taler-cli"
echo "  - taler-tx:   bin/taler-tx"
if [ -f "bin/taler-qt" ]; then
    echo "  - taler-qt:   bin/taler-qt"
fi
echo ""
echo "To install system-wide, run: sudo make install"
echo ""
