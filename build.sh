#!/bin/sh
# RetroSuite PSP build script.
#   ./build.sh            — configure (if needed) + build
#   ./build.sh clean      — wipe the build directory
#   ./build.sh static     — build with cores linked statically
set -e

export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

BUILD_DIR=build
EXTRA_ARGS=""

case "$1" in
  clean)
    rm -rf "$BUILD_DIR" build-static
    echo "cleaned"
    exit 0
    ;;
  static)
    BUILD_DIR=build-static
    EXTRA_ARGS="-DRS_STATIC_CORES=ON"
    ;;
esac

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  psp-cmake -S . -B "$BUILD_DIR" $EXTRA_ARGS
fi
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo
echo "EBOOT: $BUILD_DIR/src/EBOOT.PBP"
