# Install RetroShell and community cores

This page is for players. No development tools are required to install a
core that has already been packaged for RetroShell.

## Install RetroShell

1. Download the latest `RetroShell-PSP-*.zip` from
   [GitHub Releases](https://github.com/dadcapgamer/retroshellpsp/releases).
2. Connect the PSP by USB or put its Memory Stick in your computer.
3. Extract the ZIP to the **root of the Memory Stick**. Merge folders if your
   computer asks.
4. Put your legally obtained ROM files anywhere inside `ROMS/`.
5. On the PSP, open **Game -> Memory Stick -> RetroShell**.

After extraction, the important paths are:

```text
PSP/GAME/RetroShell/EBOOT.PBP
RETROSHELL/cores/
ROMS/
```

RetroShell requires custom firmware. Games and BIOS files are not included.

## Where to get cores

The normal RetroShell ZIP contains the cores explicitly marked as bundled for
that release. Some Testing adapters are source-only until their native binary
passes packaging and hardware qualification, so check the release notes and
emulator directory rather than relying on the status label alone.

Official standalone core packages, when available, appear as `.rscore.zip`
assets on [RetroShell Releases](https://github.com/dadcapgamer/retroshellpsp/releases).
A community developer may also publish a compatible package with its own
source code and release notes. Do not use the RetroArch buildbot: those files
are not RetroShell packages.

## Install a community core

RetroShell community cores are distributed as files ending in
`.rscore.zip`. Only install packages made specifically for RetroShell.

1. Download the core's `.rscore.zip` package from a source you trust.
2. Copy the **unopened ZIP** into `RETROSHELL/cores/` on the PSP Memory Stick.
3. Fully close and reopen RetroShell.
4. RetroShell validates and installs the package automatically.

Older beta instructions placed packages directly in `RETROSHELL/`. That
location remains supported for compatibility, but `RETROSHELL/cores/` is the
standard location going forward.

After a successful installation, RetroShell removes the copied installer ZIP
to avoid storing both the compressed package and extracted PRX. It retains the
small license and provenance records. A rejected ZIP remains untouched so its
error can be investigated in the log.

A correctly installed in-process core has two matching files:

```text
RETROSHELL/cores/example.prx
RETROSHELL/cores/example.json
```

The package may also add its license and source information under
`RETROSHELL/licenses/`. RetroShell discovers the new core during startup; a
library rescan is not required.

RetroShell also supports native PSP emulator packages. They use the same
`.rscore.zip` installation flow, but install an EBOOT and its required files
under `RETROSHELL/emulators/<name>/`. The core picker labels these **NATIVE**.
When launched, RetroShell records the game, closes itself, and passes the ROM
path to the emulator. The emulator therefore retains its original PSP audio,
video, timing, memory, suspend, and dynarec behavior. Protocol-v1 adapters can
return directly to RetroShell; older adapters may still return to the XMB.

When more than one installed core supports a game, use the selected game's
**Options** menu to choose which core launches it. RetroShell remembers the
choice for that game.

## Cores that need a BIOS file

Some emulators cannot run without the original console's BIOS. RetroShell does
not include BIOS files and never will: they are copyrighted, and dumping one
from hardware you own is the only route the project can endorse. A core that
needs one stays unusable until you supply it.

**FrogGBA** is the current example. It requires a Game Boy Advance BIOS.

1. Obtain `gba_bios.bin` from a GBA you own. It must be **exactly 16,384
   bytes** (16 KB).
2. Create `RETROSHELL/system/` on the Memory Stick if it does not exist.
3. Copy the file there, named exactly `gba_bios.bin`:

```text
RETROSHELL/system/gba_bios.bin
```

4. Install the FrogGBA package as normal, then pick it for a game through
   **Options -> Per-game Settings**.

RetroShell verifies the size before every launch and copies the file into the
emulator's own folder, so you maintain one copy rather than one per emulator.
You do not need to place it under `RETROSHELL/emulators/froggba/` yourself.

If the file is missing or the wrong size, the launch is refused with a message
naming the cause rather than an error code:

| Message | Meaning |
| --- | --- |
| `froggba: needs a valid BIOS in RETROSHELL/system` | No `gba_bios.bin`, or it is not 16 KB |
| `froggba: could not create its working folders` | The emulator's `roms`/`save`/`state`/`cfg`/`cheat`/`snapshot` folders could not be made — usually a full or write-protected Memory Stick |
| `froggba: emulator is not installed` | The package did not install; check `RETROSHELL/emulators/froggba/EBOOT.PBP` exists |

A wrong-size file is the common mistake. Headered dumps, zipped files renamed
to `.bin`, and BIOS images from other consoles all fail this check.

## What cannot be installed directly

These files are **not** interchangeable with RetroShell cores:

- RetroArch `.so` core downloads;
- Windows, macOS, Android, or desktop emulator builds;
- standalone PSP emulators that do not accept a ROM launch path and have not
  received a small source-level launch adapter;
- a `.prx` without its matching RetroShell `.json` manifest.

An emulator must either implement the RetroShell PRX API or be packaged as a
validated native EBOOT that accepts the selected ROM as `argv[1]`. Players can
then install the finished `.rscore.zip` without compiling anything.

## Compatibility labels

Every community-core download should carry one of these status labels:

| Label | Meaning |
| --- | --- |
| **Included** | Shipped with the normal RetroShell download. |
| **Testing** | Works, but still needs broader real-hardware testing. |
| **Experimental** | May be unstable, slow, or incompatible with some games. |
| **Any PSP** | Has passed the memory gate for the 32 MB PSP-1000. |
| **Later PSPs** | Intended for 64 MB PSP-2000, 3000, Go, and Street models. |

RetroShell shows every installed, non-blocklisted core that declares support
for the selected game's system. The label is guidance rather than a gate:
**Any PSP** has passed the PSP-1000 memory gate, while **Testing** and
**Later PSPs** warn that broader hardware qualification is incomplete.

## Current core status

| System | Core | Status | Model support |
| --- | --- | --- | --- |
| Game Boy / Game Boy Color | Gambatte | Included | Any PSP |
| NES | QuickNES | Included | Any PSP |
| Genesis / Mega Drive | PicoDrive | Testing | Any PSP |
| Super Nintendo | Snes9x 2005 | Testing | Any PSP; PSP-1000 testing continues |
| Super Nintendo | Snes9xTYL ME native | Adapter source; package pending | Any PSP; direct-launch qualification pending |
| Game Boy Advance | gpSP | Testing | Later PSPs; PSP-1000 experimental |
| Game Boy Advance | FrogGBA native | Adapter source; package pending | Any PSP; RetroShell launch qualification pending |
| PC Engine | Beetle PCE Fast | Testing | Later PSPs; PSP-1000 experimental |
| Master System / Game Gear | SMS Plus GX | Experimental | Later PSPs; PSP-1000 unverified |

These labels describe the current beta, not a guarantee that every game is
compatible.

FrogGBA requires a legally obtained GBA BIOS at
`RETROSHELL/system/gba_bios.bin`. Before native launch, RetroShell synchronizes
that file into FrogGBA's private folder when needed. RetroShell does not
distribute BIOS files.

Where more than one installed core supports a system, press **Triangle** on a
game and choose **Core**. RetroShell shows every installed, non-blocklisted
match; it does not hide experimental alternatives. The highest-priority safe
core remains the automatic default.

## If a core does not appear

Check the following:

- The `.prx` and `.json` have exactly the same base filename.
- Both files are directly inside `RETROSHELL/cores/`.
- The core package supports the ROM's console.
- RetroShell was fully restarted after copying the files.
- The package is compatible with the installed RetroShell version.

PRX packages declare the Core API they were built for. Native-emulator packages
declare a separate package format and executable path. After an app update,
download a current package if the log reports an incompatible descriptor.

The log at `RETROSHELL/retroshell.log` records which manifests were accepted
or rejected. Include that file when reporting an installation problem.

## Native-code safety

Core PRX and emulator EBOOT files are native PSP programs, not sandboxed themes
or plugins. A malicious or broken package can corrupt saves or crash the system. Prefer
packages that publish their source, upstream commit, license, PSP model
support, and reproducible artifact hash.

Developers who want to port or package an emulator should read
[Adding an emulator core](ADDING_A_CORE.md).
