# RetroShell emulator directory

RetroShell supports both in-process PRX cores and process-replacement native
PSP emulators, packaged as `.rscore.zip` files. The current downloadable directory is published with each compatible
[GitHub release](https://github.com/dadcapgamer/retroshellpsp/releases).

To install one, copy the **unopened** `.rscore.zip` into `RETROSHELL/cores/` on the
PSP Memory Stick and restart RetroShell. The package is validated, installed,
and consumed automatically. No manual PRX copying is required.

Packages placed directly in `RETROSHELL/` are still recognized for backward
compatibility.

| System | Core | Status | PSP models | Availability |
| --- | --- | --- | --- | --- |
| Game Boy / Game Boy Color | Gambatte | Included | Any PSP | Beta ZIP + standalone package |
| NES | QuickNES | Included | Any PSP | Beta ZIP + standalone package |
| Genesis / Mega Drive | PicoDrive | Testing | Any PSP | Beta ZIP + standalone package |
| Super Nintendo | Snes9x 2005 | Testing | Any PSP; PSP-1000 testing continues | Beta ZIP + standalone package |
| Super Nintendo | Snes9xTYL ME native | Testing alternate | Any PSP | Adapter source; binary package pending |
| Game Boy Advance | gpSP | Testing | Later PSPs; PSP-1000 experimental | Beta ZIP + standalone package |
| Game Boy Advance | FrogGBA native | Testing alternate | Any PSP; launch and return confirmed on PSP-1000 | Adapter source; binary package pending |
| PC Engine | Beetle PCE Fast | Testing | Later PSPs; PSP-1000 experimental | Beta ZIP + standalone package |
| Master System / Game Gear | SMS Plus GX | Experimental | Later PSPs; PSP-1000 unverified | Beta ZIP + standalone package |

PicoDrive also exposes experimental Master System and Game Gear support.
Compatibility labels describe the current RetroShell build, not every game.

Every installed core that declares the selected system is shown in the
per-game core picker. The Beta 3 download provides multi-core coverage for
SMS/Game Gear (PicoDrive or SMS Plus GX). Native GBA and SNES alternatives
appear only when a separately qualified adapter package is installed.
Additional candidates will be published only after they can load, save, and
exit without crashing.

## Not offered

The following integrations remain in the source tree for audit history but
are deliberately not downloadable:

| Core | Reason |
| --- | --- |
| FCEUmm | PSP performance and state-path failures. |
| Gearboy | Sustained PSP-1000 audio starvation and low speed. |
| Snes9x 2005 Plus | Failed the PSP-1000 performance gate. |
| TGB Dual | Video output stopped during PSP-1000 testing. |
| mGBA | Crashed real PSP hardware during GBA testing. |

FrogGBA requires a user-supplied `RETROSHELL/system/gba_bios.bin` of exactly
16,384 bytes; BIOS files are not included. RetroShell verifies the size and
copies it into FrogGBA's private folder before launching, so one copy serves
every emulator that needs it. Step-by-step setup and the exact failure
messages are in [Installing cores](INSTALLING_CORES.md#cores-that-need-a-bios-file).

## Package trust

A core PRX is native executable code. RetroShell rejects malformed archives,
encrypted entries, unsafe paths, oversized payloads, and mismatched package
metadata, but it cannot make untrusted native code safe. Install packages
only from a source you trust.

Developers can follow [Adding an emulator core](ADDING_A_CORE.md) to create a
reproducible package for review.
