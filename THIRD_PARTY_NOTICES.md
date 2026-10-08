# Third-party core notices

RetroShell's frontend is MIT-licensed. Emulator cores remain under their
upstream licenses; the release archive includes the corresponding retained
license text for every PRX.

The interface is set in IBM Plex Mono, Copyright © 2017 IBM Corp., licensed
under the SIL Open Font License 1.1. The complete license is stored at
`assets/fonts/OFL-IBMPlexMono.txt`.

| Core | Upstream | License |
|---|---|---|
| Gambatte | libretro/gambatte-libretro | GPL-2.0 |
| Gearboy | drhelius/Gearboy | GPL-3.0 |
| gpSP | libretro/gpsp | GPL-2.0 |
| FrogGBA (archived PRX conversion) | tzubertowski/FrogGBA, adapted through the TempGBA libretro interface | GPL-2.0 |
| mGBA (archived source) | libretro/mgba | MPL-2.0 |
| Beetle PCE Fast | libretro/beetle-pce-fast-libretro | GPL-2.0 |
| QuickNES | libretro/QuickNES_Core | GPL-2.0 |
| TGB Dual | libretro/tgbdual-libretro | GPL-2.0 |
| FCEUmm | libretro/libretro-fceumm | GPL-2.0 |
| Snes9x 2005 | libretro/snes9x2005 | Snes9x non-commercial license |
| Snes9x 2005 Plus | libretro/snes9x2005 | Snes9x non-commercial license |
| PicoDrive | libretro/picodrive | GPL-2.0 |
| SMS Plus GX | libretro/smsplus-gx | GPL-2.0 |

## Bundled native emulators

The release also ships two standalone PSP emulators in
`RETROSHELL/emulators/`. Each runs as its own program, launched by RetroShell
and returning to it; their license texts are installed as
`RETROSHELL/licenses/<name>-COPYRIGHT.txt`.

| Emulator | Upstream | License | RetroShell changes |
|---|---|---|---|
| FrogGBA | tzubertowski/FrogGBA (commit 9d75721) | GPL-2.0 | `native-emulators/froggba/patches/` |
| Snes9xTYL ME | OniMock/snes9xTYL (commit f9436e4) | Snes9x non-commercial license | `native-emulators/snes9xtyl/*.patch` |

Snes9x is copyright its authors and is distributed free of charge, as its
license requires; RetroShell is not sold. The pinned commits, patches and
payload hashes are recorded in each `native-emulators/<name>/adapter.json`
and the package's `provenance.json`. No BIOS or game files are included.

The exact repositories, commits, retained-source hashes, patches, build flags,
and expected PRX hashes are recorded in `core-provenance.lock.json`. This
source repository is the corresponding-source distribution for the local
builds, including the PSP integration shim and upstream PSP core build recipes.
