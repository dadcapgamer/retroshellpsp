# Install RetroShell

RetroShell requires a PSP with custom firmware.

1. Download `RetroShell-PSP-v1.0.0-beta.1.zip` from GitHub Releases.
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

To add cover art, place a PNG or JPG beside the ROM using the same base name:

```text
Pokemon Yellow.gb
Pokemon Yellow.png
```

The in-game RetroShell menu opens with **L + R + Select**.
