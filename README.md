# RetroShell PSP

RetroShell is an open-source retro game launcher for the Sony PSP. It scans one ROM library, organizes games by system, and loads PSP-native emulator cores from a single interface.

The project targets both the 32 MB PSP-1000 and later 64 MB models. It requires custom firmware.

> **Beta:** `v1.0.0-beta.1` is the first public test release. Save data should be backed up before updating.

## Install

1. Download `RetroShell-PSP-v1.0.0-beta.1.zip` from [GitHub Releases](https://github.com/dadcapgamer/retroshellpsp/releases).
2. Extract the ZIP to the root of the PSP Memory Stick.
3. Put ROM files anywhere inside `ms0:/ROMS/`.
4. Open **Game → Memory Stick → RetroShell** on the PSP.

The installed files should look like this:

```text
ms0:/PSP/GAME/RetroShell/EBOOT.PBP
ms0:/RETROSHELL/cores/
ms0:/ROMS/
```

Games and BIOS files are not included.

Existing `RETROSUITE` data is migrated to `RETROSHELL` on first boot. Saves, settings, artwork, logs, and installed cores are preserved.

## Core status

| System | Core | Beta status |
| --- | --- | --- |
| Game Boy / Game Boy Color | Gambatte | Included |
| NES | QuickNES | Included |
| Game Boy Advance | gpSP | Testing |
| Genesis / Mega Drive | PicoDrive | Testing |
| Super Nintendo | Snes9x 2005 | Testing on PSP-1000 |
| PC Engine | Beetle PCE Fast | Testing |
| Master System / Game Gear | PicoDrive / SMS Plus GX | Experimental |

Testing and experimental cores may be hidden by **PSP-1000 Safe Mode**. Core status will change only after real-hardware testing.

## ROMs and cover art

RetroShell scans `ms0:/ROMS/` recursively. Folder names do not determine the system.

For cover art, place a PNG or JPG beside the ROM using the same base name:

```text
Pokemon Yellow.gb
Pokemon Yellow.png
```

Open the in-game RetroShell menu with **L + R + Select**.

## Build

Install [PSPDEV](https://github.com/pspdev/pspdev), then run:

```sh
./build.sh
./build.sh test
./build.sh candidates
```

The candidate command creates the versioned PSP ZIP and optional core packages under `dist/`.

## Contributing

- [Architecture](docs/ARCHITECTURE.md)
- [Adding a core](docs/ADDING_A_CORE.md)
- [Core audit and hardware status](docs/CORE_AUDIT.md)

Do not submit copyrighted ROMs, BIOS files, or keys.

## License

RetroShell is MIT licensed. Emulator cores and vendored libraries retain their upstream licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
