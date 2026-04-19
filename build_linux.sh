#!/bin/bash
set -e

NCPU=$(nproc)

echo "=== Taler Linux Build (BDB 18, system Qt/Boost) ==="
echo ""

# Check build tools
echo "Checking build tools..."
MISSING=""
for tool in autoconf automake libtoolize pkg-config make g++; do
  if ! command -v $tool >/dev/null 2>&1; then
    MISSING="$MISSING $tool"
  fi
done
if [ -n "$MISSING" ]; then
  echo "Missing build tools:$MISSING"
  echo "Install with: sudo apt-get install build-essential libtool autotools-dev automake pkg-config"
  exit 1
fi
echo "All build tools found."

# Install system dependencies (matches .github/workflows/build-linux-x64.yml)
if [ "$1" = "--install-deps" ]; then
  echo ""
  echo "Installing system dependencies..."
  sudo apt-get update
  sudo apt-get install -y \
    build-essential libtool autotools-dev automake pkg-config \
    libssl-dev libevent-dev bsdmainutils python3 \
    libboost-system-dev libboost-filesystem-dev libboost-chrono-dev \
    libboost-test-dev libboost-thread-dev \
    qtbase5-dev qttools5-dev qttools5-dev-tools \
    protobuf-compiler libprotobuf-dev \
    libqrencode-dev libzmq3-dev \
    xvfb
else
  echo ""
  echo "Checking system libraries..."
  MISSING_PC=""
  for pc in Qt5Core Qt5Gui Qt5Network Qt5Widgets openssl libevent libzmq protobuf libqrencode; do
    if ! pkg-config --exists "$pc" 2>/dev/null; then
      MISSING_PC="$MISSING_PC $pc"
    fi
  done
  if [ ! -f /usr/include/boost/version.hpp ]; then
    MISSING_PC="$MISSING_PC boost"
  fi
  if [ -n "$MISSING_PC" ]; then
    echo "Missing system libraries:$MISSING_PC"
    echo "Re-run the script with --install-deps to install them automatically:"
    echo "  ./build_linux.sh --install-deps"
    exit 1
  fi
  echo "All system libraries found."
fi

# Build Berkeley DB 18.1.40 if not already present
if [ ! -d "$PWD/db18" ]; then
  echo ""
  echo "Building Berkeley DB 18.1.40..."
  chmod +x contrib/install_db18.sh
  ./contrib/install_db18.sh "$PWD"
else
  echo ""
  echo "Berkeley DB 18 already built at $PWD/db18, skipping."
fi

# Generate configure if needed
if [ ! -f configure ]; then
  echo ""
  echo "Running autogen.sh..."
  ./autogen.sh
fi

# Configure
echo ""
echo "Configuring..."
./configure \
  --with-incompatible-bdb \
  --with-gui \
  BDB_LIBS="-L$PWD/db18/lib -ldb_cxx-18.1" \
  BDB_CFLAGS="-I$PWD/db18/include" \
  CXXFLAGS="-O2"

# Force static Boost linking (portable binary across Ubuntu versions)
sed -i 's|-lboost_\([a-z_]*\)|-l:libboost_\1.a|g' src/Makefile

# Build
echo ""
echo "Building Taler with $NCPU parallel jobs..."
make -j${NCPU}

# Copy binaries
mkdir -p bin
cp src/talerd bin/ 2>/dev/null || true
cp src/taler-cli bin/ 2>/dev/null || true
cp src/taler-tx bin/ 2>/dev/null || true
cp src/qt/taler-qt bin/ 2>/dev/null || true

echo ""
echo "=== Build complete ==="
echo "Binaries in ./bin/"
ls -lh bin/
echo ""
echo "=== Binary info ==="
if [ -f bin/talerd ]; then
  file bin/talerd
  ./bin/talerd --version | head -1 || true
fi
if [ -f bin/taler-qt ]; then
  file bin/taler-qt
fi
