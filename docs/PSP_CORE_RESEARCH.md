# PSP core research and candidate roadmap

Research snapshot: 2026-07-26

## What the RetroArch PSP catalog tells us

RetroArch's current PSP buildbot is a useful source of candidate cores, but
not a compatibility certification. A listed core is known to compile for the
PSP target. It may still be too slow, consume too much memory, or fail games
on a 32 MB PSP-1000.

Current official PSP artifacts relevant to RetroSuite include:

| System | Current PSP buildbot cores | RetroSuite evaluation order |
| --- | --- | --- |
| GB/GBC | Gambatte, Gearboy, TGB Dual | Gambatte, Gearboy, TGB Dual |
| NES | FCEUmm, QuickNES, Nestopia | FCEUmm, QuickNES, Nestopia |
| SNES | Snes9x 2005, Snes9x 2005 Plus | 2005 Plus, 2005, PSP-specific TYL research |
| Mega Drive / Sega CD / 32X | PicoDrive | PicoDrive |
| Master System / Game Gear | SMS Plus, Gearsystem | SMS Plus, Gearsystem |
| GBA | gpSP, mGBA | gpSP, then mGBA as a compatibility comparison |
| PC Engine | Beetle PCE, Beetle PCE Fast | Beetle PCE Fast |
| Atari Lynx | Handy, Beetle Lynx | Handy, then Beetle Lynx |
| Neo Geo Pocket | RACE, Beetle NeoPop | RACE, then Beetle NeoPop |
| WonderSwan | Beetle WonderSwan | Beetle WonderSwan |
| MSX | blueMSX, fMSX | fMSX, then blueMSX |
| Arcade | MAME 2003, MAME 2003 Plus | Deferred; test curated games only |

Notably, the current PSP buildbot does **not** publish Snes9x 2010,
Genesis Plus GX, PCSX-ReARMed, or a Nintendo 64 core. These should not become
production defaults merely because they work on desktop or PPSSPP.

## Recommended first candidate wave

Do not install every buildbot core into production. Build the following as
test-only PRX packages and let the same ROM corpus select winners:

1. **GB/GBC:** Gambatte is the accuracy-oriented default. Gearboy is the
   compatibility alternate. TGB Dual is retained only if its dual-system
   behavior provides a measured benefit that justifies its memory cost.
2. **NES:** FCEUmm is the compatibility baseline. QuickNES is the speed
   candidate. Nestopia is the accuracy comparison and is expected to cost
   more CPU and memory.
3. **SNES:** Snes9x 2005 Plus is the immediate missing candidate. Keep
   Snes9x 2005 as a comparison, not a presumed default. Snes9x 2010 remains
   experimental. Investigate Snes9xTYL's PSP GU renderer, Media Engine audio,
   and game-specific speed work for portable optimizations.
4. **Sega:** PicoDrive remains the Mega Drive/Sega CD/32X baseline. SMS Plus
   is the dedicated Master System/Game Gear candidate; Gearsystem is the
   alternate.
5. **GBA:** gpSP is the first performance candidate because of its dynarec.
   mGBA is a compatibility comparison, not an expected PSP-1000 default.
   Both remain blocked until host VFS/streaming is proven.
6. **PC Engine:** Beetle PCE Fast is the first candidate after streaming and
   shared memory work pass.

The second wave can cover Lynx, Neo Geo Pocket, WonderSwan, Atari 8-bit/7800,
ColecoVision, and MSX. Arcade must use a curated game set because a core
binary existing does not imply its ROM sets fit the PSP-1000 budget.

PS1 should use the PSP's native POPS route rather than spending RetroSuite's
limited memory on a libretro PS1 core. N64 is outside the production scope
until a PSP-1000 result proves otherwise.

## Compatibility decision rule

All candidates install as `testOnly` and receive equal tests. A core becomes
the default only after it:

- boots and renders non-static, non-black video on a real PSP-1000;
- sustains the native frame budget without persistent audio underruns;
- survives 20 launch/exit and core-switch cycles;
- preserves SRAM and states across interrupted-write tests;
- has no arena allocation failure or unbounded memory growth;
- passes a representative base-game and enhancement-chip ROM corpus.

The frontend's frame counter is not proof of video output. Initial-frame
telemetry must also record changing-pixel and all-black detection.

## Community core catalog ("marketplace")

The community idea fits RetroSuite well if it begins as a curated,
open catalog rather than an in-PSP commercial store.

Each installable package should contain:

```text
<core-id>/
  core.prx
  manifest.json
  LICENSE.txt
  provenance.json
  README.txt
```

Manifest version 2 should add:

- package format and RetroSuite Core API versions;
- core id, version, systems, extensions, priority, and test-only status;
- PSP-1000 support declaration and measured memory high-water;
- SHA-256 hashes for every packaged file;
- publisher name and signing key id;
- source repository, exact commit, license, and corresponding-source URL;
- required BIOS names and hashes, without distributing copyrighted BIOSes;
- known compatibility exceptions and safe core options.

Because a PRX is native executable code with no meaningful PSP sandbox,
RetroSuite must never silently trust a downloaded core. The official catalog
should accept reproducible packages through review, verify hashes/signatures,
show publisher and PSP-1000 status, and refuse incompatible API versions.
Manual unsigned drag-and-drop packages can remain possible behind a clear
warning for developers.

The first release should be a Git-hosted catalog plus a desktop packaging and
verification tool. Network downloading on the PSP can come later; it adds TLS,
parser, interrupted-download, and memory risks without helping core emulation.

## Sources

- RetroArch PSP compilation:
  https://docs.libretro.com/development/retroarch/compilation/psp/
- Current PSP build artifacts:
  https://buildbot.libretro.com/nightly/playstation/psp/latest/
- RetroArch PSP memory-related changes:
  https://github.com/libretro/RetroArch/blob/master/CHANGES.md
- Libretro frontend/core contract:
  https://docs.libretro.com/development/frontends/
- Gambatte core documentation:
  https://docs.libretro.com/library/gambatte/
- gpSP core documentation:
  https://docs.libretro.com/library/gpsp/
- Snes9x 2005 source:
  https://github.com/libretro/snes9x2005
- Snes9xTYL PSP source:
  https://github.com/esmjanus/snes9xTYL

