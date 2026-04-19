#!/bin/bash
set -e

NCPU=$(nproc)
HOST=x86_64-w64-mingw32

echo "=== Taler Windows x64 Build (cross-compile from Linux via MinGW-w64) ==="
echo ""

# Install cross-compile dependencies (matches .github/workflows/build-windows-x64.yml)
# This block must run before the tool check so --install-deps works on a fresh machine.
if [ "$1" = "--install-deps" ]; then
  echo "Installing cross-compile dependencies..."
  sudo apt-get update
  sudo apt-get install -y \
    build-essential libtool autotools-dev automake pkg-config \
    bsdmainutils curl git \
    g++-mingw-w64-x86-64 nsis
  echo ""
  echo "Setting MinGW POSIX threading model..."
  sudo update-alternatives --set ${HOST}-g++ /usr/bin/${HOST}-g++-posix
  echo ""
fi

# Check build tools
echo "Checking build tools..."
MISSING=""
for tool in autoconf automake libtoolize pkg-config make g++ ${HOST}-g++; do
  if ! command -v $tool >/dev/null 2>&1; then
    MISSING="$MISSING $tool"
  fi
done
if [ -n "$MISSING" ]; then
  echo "Missing build tools:$MISSING"
  echo "Re-run the script with --install-deps to install them automatically:"
  echo "  ./build_windows.sh --install-deps"
  exit 1
fi
echo "All build tools found."

# Ensure MinGW alternatives point to the POSIX variant (threads needed by Boost)
CURRENT_GXX=$(update-alternatives --query ${HOST}-g++ 2>/dev/null | awk '/^Value:/ {print $2}')
if [ -n "$CURRENT_GXX" ] && [ "$CURRENT_GXX" != "/usr/bin/${HOST}-g++-posix" ]; then
  echo ""
  echo "MinGW ${HOST}-g++ currently points to: $CURRENT_GXX"
  echo "Switching to the POSIX threading variant (required by Boost)..."
  sudo update-alternatives --set ${HOST}-g++ /usr/bin/${HOST}-g++-posix
fi

# Build all dependencies from source (static, via depends/)
echo ""
echo "Building dependencies from source via depends/ (first run takes 30-60 min)..."
echo "Using $NCPU parallel jobs, HOST=$HOST"
# Retry once on failure: Qt 5.15 has a moc/plugin parallel race; the second
# pass uses cached .o files and completes the remaining work deterministically.
make -C depends HOST=$HOST -j${NCPU} || make -C depends HOST=$HOST -j${NCPU}

# Regenerate configure if the scripts or M4 macros changed (or if configure is missing)
NEED_AUTOGEN=0
if [ ! -f configure ]; then
  NEED_AUTOGEN=1
else
  for src in configure.ac configure.in build-aux/m4/*.m4 $(find . -name Makefile.am -not -path './depends/*' 2>/dev/null); do
    [ -f "$src" ] && [ "$src" -nt configure ] && { NEED_AUTOGEN=1; break; }
  done
fi
if [ "$NEED_AUTOGEN" = "1" ]; then
  echo ""
  echo "Running autogen.sh (configure or its inputs changed)..."
  ./autogen.sh
fi

# Configure with depends/ prefix
echo ""
echo "Configuring..."
CONFIG_SITE=$PWD/depends/${HOST}/share/config.site \
  ./configure --prefix=/

# Build
echo ""
echo "Building Taler with $NCPU parallel jobs..."
make -j${NCPU}

# Copy binaries
mkdir -p bin
cp src/talerd.exe bin/ 2>/dev/null || true
cp src/taler-cli.exe bin/ 2>/dev/null || true
cp src/taler-tx.exe bin/ 2>/dev/null || true
cp src/qt/taler-qt.exe bin/ 2>/dev/null || true

echo ""
echo "=== Build complete ==="
echo "Binaries in ./bin/"
ls -lh bin/
echo ""
echo "=== Binary info ==="
for b in bin/talerd.exe bin/taler-qt.exe; do
  if [ -f "$b" ]; then
    file "$b"
  fi
done
