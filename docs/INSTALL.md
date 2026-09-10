# Install RetroShell

RetroShell requires a PSP with custom firmware.

1. Download the latest `RetroShell-PSP-*.zip` from GitHub Releases.
2. Extract the ZIP to the root of the PSP Memory Stick.
3. Put ROM files anywhere inside `ms0:/ROMS/`.
4. On the PSP, open **Game → Memory Stick → RetroShell**.

After extraction, these paths must exist:

```text
ms0:/PSP/GAME/RetroShell/EBOOT.PBP
ms0:/RETROSHELL/cores/
ms0:/ROMS/
```

Games and BIOS files are not included.

When updating, merge the new folders into the existing installation. Do not
delete `ms0:/RETROSHELL/system/`: it contains user-supplied BIOS files and is
never replaced by RetroShell release packages. ROMs, saves, settings, artwork,
and BIOS files should be backed up before manually deleting an installation.

The optional FrogGBA core requires a legally obtained GBA BIOS at
`ms0:/RETROSHELL/system/gba_bios.bin`. gpSP remains the default GBA core and
does not require this file with RetroShell's current build.

To add cover art, place a PNG or JPG beside the ROM using the same base name:

```text
Pokemon Yellow.gb
Pokemon Yellow.png
```

The in-game RetroShell menu opens with **L + R + Select**.
