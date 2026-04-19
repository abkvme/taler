#!/bin/bash
set -e

NCPU=$(sysctl -n hw.ncpu)

echo "=== Taler macOS Build (static, depends-based) ==="
echo ""

# Only build tools needed from Homebrew (not libraries)
echo "Checking build tools..."
MISSING=""
for tool in automake libtool pkg-config; do
  if ! command -v $tool >/dev/null 2>&1; then
    MISSING="$MISSING $tool"
  fi
done
if [ -n "$MISSING" ]; then
  echo "Missing build tools:$MISSING"
  echo "Install with: brew install$MISSING"
  exit 1
fi
echo "All build tools found."

# Build all dependencies from source (static)
echo ""
echo "Building dependencies from source (first run takes 15-30 min)..."
echo "Using $NCPU parallel jobs"
make -C depends -j${NCPU}

# Detect host triplet
HOST_TRIPLET=$(cd depends && ./config.guess)
echo ""
echo "Host triplet: ${HOST_TRIPLET}"
echo "Config site:  depends/${HOST_TRIPLET}/share/config.site"

# Generate configure if needed
if [ ! -f configure ]; then
  echo ""
  echo "Running autogen.sh..."
  ./autogen.sh
fi

# Configure with depends/ prefix
echo ""
echo "Configuring..."
CONFIG_SITE=$PWD/depends/${HOST_TRIPLET}/share/config.site \
  ./configure --prefix=/ --with-incompatible-bdb

# Build
echo ""
echo "Building Taler..."
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
echo "=== Dynamic library check ==="
if [ -f bin/taler-qt ]; then
  otool -L bin/taler-qt | head -20
  if otool -L bin/taler-qt | grep -q "/opt/homebrew\|/usr/local/opt"; then
    echo ""
    echo "WARNING: Non-system dynamic libraries detected!"
  else
    echo ""
    echo "OK: All libraries are system-only (fully static build)"
  fi
fi
