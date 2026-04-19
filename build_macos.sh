#!/bin/bash
set -e

NCPU=$(sysctl -n hw.ncpu)

echo "=== Taler macOS Build (static, depends-based) ==="
echo ""

# Handle clean subcommands before any tool checks
if [ "$1" = "clean" ]; then
  echo "Cleaning Taler build state (keeping depends/ prefix to save ~30 min)..."
  [ -f Makefile ] && make distclean 2>/dev/null || true
  rm -rf config.log config.status configure.scan autom4te.cache
  rm -rf bin
  find . -name '*.o' -not -path './depends/*' -delete 2>/dev/null || true
  find . -name '*.lo' -not -path './depends/*' -delete 2>/dev/null || true
  find . -name '.deps' -type d -not -path './depends/*' -exec rm -rf {} + 2>/dev/null || true
  rm -f src/qt/moc_*.cpp src/qt/ui_*.h src/qt/qrc_*.cpp
  echo "Clean complete. Run ./build_macos.sh to rebuild."
  exit 0
fi

if [ "$1" = "clean-all" ]; then
  echo "Full clean (removes all cached depends builds — next build will take ~30 min)..."
  [ -f Makefile ] && make distclean 2>/dev/null || true
  rm -rf config.log config.status configure.scan autom4te.cache configure
  rm -rf bin
  rm -rf depends/work depends/built depends/sdk-sources depends/sources
  rm -rf depends/arm-apple-darwin* depends/x86_64-apple-darwin*
  rm -rf depends/x86_64-linux-gnu* depends/aarch64-linux-gnu*
  rm -rf depends/x86_64-w64-mingw32
  rm -rf depends/native
  find . -name '*.o' -not -path './depends/*' -delete 2>/dev/null || true
  find . -name '*.lo' -not -path './depends/*' -delete 2>/dev/null || true
  find . -name '.deps' -type d -not -path './depends/*' -exec rm -rf {} + 2>/dev/null || true
  rm -f src/qt/moc_*.cpp src/qt/ui_*.h src/qt/qrc_*.cpp
  echo "Full clean complete. Run ./build_macos.sh to rebuild from scratch."
  exit 0
fi

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
# Retry once on failure: Qt 5.15 has a moc/plugin parallel race; the second
# pass uses cached .o files and completes the remaining work deterministically.
make -C depends -j${NCPU} || make -C depends -j${NCPU}

# Detect host triplet
HOST_TRIPLET=$(cd depends && ./config.guess)
echo ""
echo "Host triplet: ${HOST_TRIPLET}"
echo "Config site:  depends/${HOST_TRIPLET}/share/config.site"

# Regenerate configure if the scripts or M4 macros changed (or if configure is missing)
NEED_AUTOGEN=0
if [ ! -f configure ]; then
  NEED_AUTOGEN=1
else
  # If any configure input is newer than configure, re-run autogen
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
