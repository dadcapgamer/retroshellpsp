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

The normal RetroShell ZIP already contains the cores listed as Included or
Testing for that release. You do not need to download those again.

Official standalone core packages, when available, appear as `.rscore.zip`
assets on [RetroShell Releases](https://github.com/dadcapgamer/retroshellpsp/releases).
A community developer may also publish a compatible package with its own
source code and release notes. Do not use the RetroArch buildbot: those files
are not RetroShell packages.

## Install a community core

RetroShell community cores are distributed as files ending in
`.rscore.zip`. Only install packages made specifically for RetroShell.

1. Download the core's `.rscore.zip` package from a source you trust.
2. Extract it to the **root of the PSP Memory Stick**.
3. Allow the `RETROSHELL` folders to merge. Do not rename the core files.
4. Fully close and reopen RetroShell.

A correctly installed core has two matching files:

```text
RETROSHELL/cores/example.prx
RETROSHELL/cores/example.json
```

The package may also add its license and source information under
`RETROSHELL/licenses/`. RetroShell discovers the new core during startup; a
library rescan is not required.

When more than one installed core supports a game, use the selected game's
**Options** menu to choose which core launches it. RetroShell remembers the
choice for that game.

## What cannot be installed directly

These files are **not** interchangeable with RetroShell cores:

- RetroArch `.so` core downloads;
- Windows, macOS, Android, or desktop emulator builds;
- standalone PSP emulators that have not been adapted to RetroShell;
- a `.prx` without its matching RetroShell `.json` manifest.

An emulator must first be ported, compiled as a PSP PRX, and packaged for
RetroShell by a developer. Players can then install the finished
`.rscore.zip` without compiling anything.

## Compatibility labels

Every community-core download should carry one of these status labels:

| Label | Meaning |
| --- | --- |
| **Included** | Shipped with the normal RetroShell download. |
| **Testing** | Works, but still needs broader real-hardware testing. |
| **Experimental** | May be unstable, slow, or incompatible with some games. |
| **Any PSP** | Has passed the memory gate for the 32 MB PSP-1000. |
| **Later PSPs** | Intended for 64 MB PSP-2000, 3000, Go, and Street models. |

PSP-1000 Safe Mode may hide a core that has not passed the 32 MB hardware
gate. Turning Safe Mode off does not make an incompatible core safe; it can
still freeze or shut down the PSP.

## Current core status

| System | Core | Status | Model support |
| --- | --- | --- | --- |
| Game Boy / Game Boy Color | Gambatte | Included | Any PSP |
| NES | QuickNES | Included | Any PSP |
| Genesis / Mega Drive | PicoDrive | Testing | Any PSP |
| Super Nintendo | Snes9x 2005 | Testing | PSP-1000 testing continues |
| Game Boy Advance | gpSP | Testing | Later PSPs; PSP-1000 experimental |
| PC Engine | Beetle PCE Fast | Testing | Later PSPs; PSP-1000 experimental |
| Master System / Game Gear | PicoDrive / SMS Plus GX | Experimental | Varies by core |

These labels describe the current beta, not a guarantee that every game is
compatible.

## If a core does not appear

Check the following:

- The `.prx` and `.json` have exactly the same base filename.
- Both files are directly inside `RETROSHELL/cores/`.
- The core package supports the ROM's console.
- RetroShell was fully restarted after copying the files.
- PSP-1000 Safe Mode is not hiding an unverified core.
- A Testing core is not being used with a production build that hides test
  packages.
- The package is compatible with the installed RetroShell version.

The log at `RETROSHELL/retroshell.log` records which manifests were accepted
or rejected. Include that file when reporting an installation problem.

## Native-code safety

Core PRX files are native PSP programs, not sandboxed themes or plugins. A
malicious or broken core can corrupt saves or crash the system. Prefer
packages that publish their source, upstream commit, license, PSP model
support, and reproducible artifact hash.

Developers who want to port or package an emulator should read
[Adding an emulator core](ADDING_A_CORE.md).
