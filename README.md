# RetroShell PSP

RetroShell is an open-source retro game launcher for the Sony PSP. It scans one ROM library, organizes games by system, and loads PSP-native emulator cores from a single interface.

The project targets both the 32 MB PSP-1000 and later 64 MB models. It requires custom firmware.

> **Beta:** `v1.0.0-beta.3` is the current public test release. Save data should be backed up before updating.

## Install

1. Download the latest `RetroShell-PSP-*.zip` from [GitHub Releases](https://github.com/dadcapgamer/retroshellpsp/releases).
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

### Install additional cores

Non-developers can install a RetroShell `.rscore.zip` package by copying the
unopened ZIP into `RETROSHELL/cores/` on the PSP Memory Stick and restarting
RetroShell. Random RetroArch or desktop cores cannot be installed directly.

See [Install RetroShell and community cores](docs/INSTALLING_CORES.md) for the
complete drag-and-drop instructions, compatibility labels, and troubleshooting.
Available downloads and rejected integrations are listed in the
[emulator directory](docs/CORE_DIRECTORY.md).

## Beta 3 highlights

- Adds a native-emulator adapter SDK for PSP emulators that need to own the
  process, display, audio, memory, and suspend lifecycle.
- Adds the protocol-v1 pause, return-to-shell, and shared save-library contract.
- Installs validated `.rscore.zip` core packages copied into `RETROSHELL/cores/`.
- Makes large libraries responsive with bounded rendering, natural sorting,
  cached artwork paths, and stable navigation.
- Adds PSP-specific video, audio-recovery, ROM-streaming, save, RTC, and
  suspend/resume hardening across the current cores.
- Redesigns the light and dark interfaces, library navigation, game lists,
  settings, pause menu, branding, and startup shimmer.

See [CHANGELOG.md](CHANGELOG.md) for the complete release summary.

## Core status

| System | Core | Beta status |
| --- | --- | --- |
| Game Boy / Game Boy Color | Gambatte | Included |
| NES | QuickNES | Included |
| Game Boy Advance | gpSP; FrogGBA adapter source | Testing; package pending |
| Genesis / Mega Drive | PicoDrive | Testing |
| Super Nintendo | Snes9x 2005 | Testing on PSP-1000 |
| PC Engine | Beetle PCE Fast | Testing |
| Master System / Game Gear | PicoDrive / SMS Plus GX | Experimental |

Every installed core that declares support for a game's system appears in the
per-game core picker. **Any PSP** means the core passed the PSP-1000 memory
gate; **Testing** means compatibility, speed, or model coverage is incomplete.
Known crash-prone integrations remain blocklisted.
FrogGBA and Snes9xTYL adapter source is included for developers, but their
native emulator packages are not release assets until the corresponding
binaries pass the packaging and hardware-qualification gates.

## ROMs and cover art

RetroShell scans `ms0:/ROMS/` recursively. Folder names do not determine the system.
After the first scan, RetroShell loads its cached index instead of walking the
entire Memory Stick on every boot. Use **Settings → Rescan library** after
adding, removing, or renaming ROM files.

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

- [Install RetroShell and community cores](docs/INSTALLING_CORES.md)
- [Emulator directory](docs/CORE_DIRECTORY.md)
- [Release changelog](CHANGELOG.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Adding a core](docs/ADDING_A_CORE.md)
- [Core audit and hardware status](docs/CORE_AUDIT.md)

Do not submit copyrighted ROMs, BIOS files, or keys.

## Credits

RetroShell was built with Fable and OpenAI Codex. Product and interface design by Dadcap.

## License

RetroShell is MIT licensed. Emulator cores and vendored libraries retain their upstream licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
