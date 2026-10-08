# Native PSP emulator adapters

RetroShell uses two emulator backends:

- **PRX core:** runs inside RetroShell through `RSCoreAPI`.
- **Native emulator:** replaces the RetroShell process through PSP loadexec and
  receives the selected ROM as `argv[1]`.

Native adapters are for PSP emulators whose performance depends on owning the
display, GU, audio channel, frame scheduler, Media Engine, volatile memory, or
suspend lifecycle. They are not a second emulation layer. RetroShell stops
running before the emulator starts.

## Current classification

| System | Default approach | Native adapter needed now |
| --- | --- | --- |
| GB / GBC | Gambatte PRX | No |
| GBA | gpSP PRX plus FrogGBA native | Yes: FrogGBA |
| NES | QuickNES PRX | No |
| SNES | Snes9x 2005 PRX plus Snes9xTYL native | Yes: Snes9xTYL |
| Genesis / Mega Drive | PicoDrive PRX | No; compare only if regressions appear |
| Master System / Game Gear | PicoDrive or SMS Plus GX PRX | No |
| PC Engine | PCE Fast PRX candidate | Not until hardware testing identifies a better native emulator |

## Package contract

A native `.rscore.zip` contains:

```text
RETROSHELL/cores/<name>.json
RETROSHELL/emulators/<name>/EBOOT.PBP
RETROSHELL/emulators/<name>/<required support files>
RETROSHELL/core-packages/<name>/package.json
RETROSHELL/core-packages/<name>/provenance.json
RETROSHELL/licenses/<name>-COPYRIGHT.txt
```

The manifest must set `backend` to `native` and use the canonical executable
path `emulators/<name>/EBOOT.PBP`. Package format 5 identifies adapters using
the versioned session/pause/return protocol; legacy process replacements use
format 4. Archive paths, expanded size, file size, system
IDs, and descriptor identity are validated before anything becomes visible.

An upstream emulator must accept its ROM path directly. If it discards launch
arguments, patch and rebuild it from pinned source; do not automate its file
browser or silently launch without the selected game.

Build a package from an already-built source directory with:

```sh
python3 tools/package_native_emulator.py froggba /path/to/FrogGBA
python3 tools/package_native_emulator.py snes9xtyl /path/to/patched/snes9xTYL
```

To build both native emulators from their pinned source and package them,
audit the repository, rebuild the EBOOT, and refresh the core-directory index
in one command:

```sh
./build.sh adapters
```

This runs `native-emulators/build_native.sh` for each emulator: it clones the
commit pinned in `adapter.json`, applies the listed patches, copies the
adapter SDK in, and builds with the pspdev toolchain in `~/pspdev` (the same
`pspdev/pspdev` image FrogGBA's Docker setup uses, so no Docker is needed).
The result is byte-for-byte identical to the packages tested on hardware.
FrogGBA's exception handler and kernel bridge are upstream's prebuilt modules
from its pinned `release/FrogGBA_v0.1.0.zip`; its `dir.ini` (snapshots kept in
the emulator's folder) and `froggba.cfg` live in
`native-emulators/froggba/files/`. Already-built trees can still be packaged
with `./build.sh adapters /path/to/FrogGBA /path/to/patched/snes9xTYL`.

Both packages are bundled into the release ZIP by `tools/package_release.py`
(`./build.sh release`): build them into `dist/core-directory/` first. A missing
native package fails the release instead of silently leaving an emulator out. The pipeline
checks the adapter manifests, payload paths, licenses, commit IDs, and package
layout; hardware qualification is still required before either adapter can be
marked included.

Snes9xTYL must be built from the commit and patch recorded in
`native-emulators/snes9xtyl/adapter.json`.

## Returning to RetroShell

Protocol v1 uses a fixed, bounded launch contract:

- `argv[1]`: selected ROM
- `argv[2]`: RetroShell EBOOT
- `argv[3]`: `ms0:/RETROSHELL/session/native-session.ini`
- pause chord: L+R+Select
- return mode: CFW loadexec

RetroShell writes the session receipt before launch. The adapter records
launch, pause, resume, and return state atomically. On its next boot,
RetroShell consumes that receipt and logs whether the session returned cleanly.
The receipt is diagnostic state, never a save state and never executable data.

### Shared save library

Protocol adapters call `rs_adapter_prepare_save_paths()` after initialization.
It creates a bounded, game-specific battery-save directory at
`RETROSHELL/saves/<System>/<pathHash>/` and a format-isolated state directory
at `RETROSHELL/saves/<System>/<pathHash>/states/<adapter>/`. The emulator must
use those paths only while launched by RetroShell; standalone launches keep
their original directories. An adapter may read its former local directory as
a legacy fallback, but new writes go to the shared library.

Save states are never copied between cores. An adapter state can only be
loaded by the emulator and compatible version that created it. Raw cartridge
saves may be importable between compatible cores, but must be size-validated
and backed up before conversion.

FrogGBA is the reference implementation. Before building its pinned source,
copy `native-emulators/sdk/retroshell_adapter.c`, `.h`, and
`src/core_api/rs_pause_menu.h` into `source/src`, then apply
`native-emulators/froggba/patches/return-to-launcher.patch`.
Snes9xTYL implements the same contract (`retroshell-adapter.patch`): L+R+Select
— or HOME, with the ME build's home hook — opens the shared pause surface,
Emulator Settings opens Snes9xTYL's own menu, battery saves go to the game's
shared folder and states, previews and cheats to `states/snes9xtyl/`, with
read-only fallback to its standalone `SAVES` folder. On first launch it still
shows Snes9xTYL's one-time licence disclaimer.

RetroShell locates its own EBOOT by probing the documented install paths
rather than calling `sceKernelInitFileName`, whose kernel-mode stub library
collides with newlib in a user module. If no candidate exists — an unusual
install location — no `argv[2]` is passed and the adapter exits to the XMB as
before. Standard ARK category installs at
`PSP/GAME/CAT_Emulators/RetroShell` are also recognized.

The pause menu is still rendered by the standalone emulator because RetroShell
is no longer resident. Every patched adapter must present the shared order:
Resume, Save State, Load State, Reset, Aspect Ratio, Filter, Screenshot,
Emulator Settings, Exit. `Emulator Settings` is the sole entry into the
emulator's original configuration UI. The adapter maps the other rows directly
to native functions. A visual RetroShell skin can be implemented inside each
adapter without changing the process or memory model. Chain-loading RetroShell
merely to draw the menu would require serializing the entire emulator and would
make resume much slower.
