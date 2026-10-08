#!/bin/sh
# Build a bundled native emulator from its pinned source and stage the
# payload that tools/package_native_emulator.py packages.
#
#   native-emulators/build_native.sh froggba   WORKDIR
#   native-emulators/build_native.sh snes9xtyl WORKDIR
#
# WORKDIR receives the clone; the staged payload is WORKDIR/<name>/stage
# (FrogGBA) or the patched source tree itself (Snes9xTYL), whose path is
# printed last. Needs the pspdev toolchain in ~/pspdev (the same image
# FrogGBA's Docker setup uses) and network access for the clone.
set -e

NAME="$1"
WORK="$2"
if [ -z "$NAME" ] || [ -z "$WORK" ]; then
  echo "usage: $0 froggba|snes9xtyl WORKDIR" >&2
  exit 2
fi
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"
export PSPSDK="$PSPDEV/psp/sdk"
ADAPTER="$ROOT/native-emulators/$NAME/adapter.json"
REPO="$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['repository'])" "$ADAPTER")"
COMMIT="$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['commit'])" "$ADAPTER")"
SRC="$WORK/$NAME"

mkdir -p "$WORK"
if [ ! -d "$SRC/.git" ]; then
  git clone --quiet "$REPO" "$SRC"
fi
git -C "$SRC" checkout --quiet --force "$COMMIT"
git -C "$SRC" clean --quiet -fdx
for patch in $(python3 -c "import json,sys;print(' '.join(json.load(open(sys.argv[1]))['patches']))" "$ADAPTER"); do
  git -C "$SRC" apply "$ROOT/$patch"
done
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 2)"

case "$NAME" in
  froggba)
    cp "$ROOT/native-emulators/sdk/retroshell_adapter.c" \
       "$ROOT/native-emulators/sdk/retroshell_adapter.h" \
       "$ROOT/src/core_api/rs_pause_menu.h" "$SRC/source/src/"
    make -C "$SRC/source" -j"$JOBS" >/dev/null
    STAGE="$SRC/stage"
    rm -rf "$STAGE"
    mkdir -p "$STAGE"
    cp "$SRC/source/EBOOT.PBP" "$SRC/source/FrogGBA.prx" "$STAGE/"
    # The exception handler and kernel bridge are upstream's own prebuilt
    # modules, shipped in its pinned release archive.
    unzip -q -j -o "$SRC/release/FrogGBA_v0.1.0.zip" \
      PSP/GAME/FrogGBA/exception.prx PSP/GAME/FrogGBA/ku_bridge.prx -d "$STAGE"
    cp "$ROOT/native-emulators/froggba/files/dir.ini" \
       "$ROOT/native-emulators/froggba/files/froggba.cfg" "$STAGE/"
    echo "$STAGE"
    ;;
  snes9xtyl)
    cp "$ROOT/native-emulators/sdk/retroshell_adapter.c" \
       "$ROOT/native-emulators/sdk/retroshell_adapter.h" \
       "$ROOT/src/core_api/rs_pause_menu.h" "$SRC/psp/"
    make -C "$SRC" mehome -j"$JOBS" >/dev/null
    make -C "$SRC/psp/homehookprx" >/dev/null
    make -C "$SRC/psp/mediaengineprx" >/dev/null
    echo "$SRC"
    ;;
  *)
    echo "unknown native emulator: $NAME" >&2
    exit 2
    ;;
esac
