#!/bin/sh
# RetroShell PSP build script.
#   ./build.sh            — configure (if needed) + build
#   ./build.sh clean      — wipe the build directory
#   ./build.sh test       — run repository audit tests
#   ./build.sh qualify LOG — grade the latest real-PSP hardware run
#   ./build.sh release    — build, audit, and create deterministic stable ZIP
#   ./build.sh candidates — build a test-core EBOOT and drag-and-drop packages
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
    echo "error: static multi-core builds are unsafe; production is PRX-only" >&2
    exit 2
    ;;
  test)
    python3 tools/audit_repository.py
    cmake -S tests/host -B build-host-tests
    cmake --build build-host-tests
    ctest --test-dir build-host-tests --output-on-failure
    cmake -S tests/fuzz -B build-fuzz
    cmake --build build-fuzz
    printf '{"malformed":[' | build-fuzz/fuzz_json
    printf 'PK\\003\\004broken' | build-fuzz/fuzz_zip
    exit 0
    ;;
  qualify)
    if [ -z "$2" ]; then
      echo "usage: ./build.sh qualify /path/to/retroshell.log" >&2
      exit 2
    fi
    python3 tools/qualify_psp_log.py "$2"
    exit $?
    ;;
  qualify-all)
    if [ -z "$2" ]; then
      echo "usage: ./build.sh qualify-all /path/to/retroshell.log" >&2
      exit 2
    fi
    python3 tools/qualify_psp_matrix_log.py "$2"
    exit $?
    ;;
  candidates)
    BUILD_DIR=build-candidates
    EXTRA_ARGS="-DRS_INCLUDE_TEST_CORES=ON"
    ;;
esac

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  psp-cmake -S . -B "$BUILD_DIR" $EXTRA_ARGS
fi
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || true)"
if [ -z "$JOBS" ] && command -v nproc >/dev/null 2>&1; then
  JOBS="$(nproc)"
fi
JOBS="${JOBS:-2}"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo
echo "EBOOT: $BUILD_DIR/src/EBOOT.PBP"

if [ "$1" = "release" ]; then
  python3 tools/audit_repository.py --build-dir "$BUILD_DIR"
  python3 tools/package_release.py --build-dir "$BUILD_DIR"
elif [ "$1" = "candidates" ]; then
  python3 tools/audit_repository.py --build-dir "$BUILD_DIR"
  python3 tools/package_release.py --build-dir "$BUILD_DIR" \
    --include-candidates --core-packages
fi
