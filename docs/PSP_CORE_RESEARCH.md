# PSP core research and candidate roadmap

Research snapshot: 2026-08-14

## What the RetroArch PSP catalog tells us

RetroArch's current PSP buildbot is a useful source of candidate cores, but
not a compatibility certification. A listed core is known to compile for the
PSP target. It may still be too slow, consume too much memory, or fail games
on a 32 MB PSP-1000.

Current official PSP artifacts relevant to RetroShell include:

| System | Current PSP buildbot cores | RetroShell evaluation order |
| --- | --- | --- |
| GB/GBC | Gambatte, Gearboy, TGB Dual, mGBA | Gambatte default; other integrations archived after hardware failures |
| NES | FCEUmm, QuickNES, Nestopia | FCEUmm, QuickNES, Nestopia |
| SNES | Snes9x 2005, Snes9x 2005 Plus | 2005 Plus, 2005, PSP-specific TYL research |
| Mega Drive / Sega CD / 32X | PicoDrive | PicoDrive |
| Master System / Game Gear | SMS Plus, Gearsystem | SMS Plus, Gearsystem |
| GBA | gpSP, mGBA, FrogGBA | gpSP performance default; FrogGBA PSP-native compatibility candidate |
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

1. **GB/GBC:** Gambatte is the PSP-1000 default. Gearboy, TGB Dual, and mGBA
   remain blocked after hardware failures; no unsafe alternate is offered.
2. **NES:** FCEUmm is the compatibility baseline. QuickNES is the speed
   candidate. Nestopia is the accuracy comparison and is expected to cost
   more CPU and memory.
3. **SNES:** Snes9x 2005 is the sole safe candidate after Snes9x 2005 Plus
   failed the PSP-1000 performance gate and Snes9x 2010 repeatedly shut the
   hardware down. The PRX now carries the safe compiler portions of
   Snes9xTYL's Allegrex profile. TYL's frontend-owned GU renderer, Media
   Engine ownership, and whole-program linking cannot be copied directly:
   RetroShell owns the GU/audio services and must unload the core cleanly.
   Game-specific wait-loop/speed-hack work is evaluated only when it has a
   compatibility test and does not compromise emulation state.
4. **Sega:** PicoDrive remains the Mega Drive/Sega CD/32X baseline. SMS Plus
   is the dedicated Master System/Game Gear candidate; Gearsystem is the
   alternate.
5. **GBA:** gpSP remains the speed-oriented default because of its PSP MIPS
   dynarec and bounded ROM page cache. mGBA was removed after crashing real
   hardware. FrogGBA replaces TempGBA as the pinned PSP-native compatibility candidate for
   games and ROM hacks gpSP rejects. It requires a real GBA BIOS and remains
   experimental until PPSSPP plus PSP-1000/64 MB hardware gates pass.
6. **PC Engine:** Beetle PCE Fast is the first candidate. Its PSP build uses
   32 kHz audio, native BGR565 output, and frontend-controlled recovery frames
   so the core can skip obsolete rendering work without skipping emulation.

The second wave can cover Lynx, Neo Geo Pocket, WonderSwan, Atari 8-bit/7800,
ColecoVision, and MSX. Arcade must use a curated game set because a core
binary existing does not imply its ROM sets fit the PSP-1000 budget.

PS1 should use the PSP's native POPS route rather than spending RetroShell's
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

The community idea fits RetroShell well if it begins as a curated,
open catalog rather than an in-PSP commercial store.

Package format 3 is now implemented. Each installable package declares its
required Core API version and contains:

```text
RETROSHELL/cores/<core-id>.prx
RETROSHELL/cores/<core-id>.json
RETROSHELL/licenses/<core-id>-<license>
RETROSHELL/core-packages/<core-id>/package.json
RETROSHELL/core-packages/<core-id>/provenance.json
RETROSHELL/core-packages/<core-id>/README.txt
```

The current descriptor records:

- package format, core id, version, systems, test-only status, and PSP-1000
  safety declaration;
- an exact PRX SHA-256;
- source repository, exact commit, retained license, patches, and compiler
  flags through the pinned provenance record;
- current Included, Testing, or Experimental status and model-support notes.

Publisher signatures, BIOS requirements, measured high-water marks, and
per-game exceptions remain future descriptor fields. They must not be implied
by the first unsigned package format.

Because a PRX is native executable code with no meaningful PSP sandbox,
RetroShell rejects malformed layouts, unsafe paths, encrypted or oversized
payloads, descriptor mismatches, the dummy core, and integrations already
disqualified by hardware testing. It installs the PRX and manifest as one
recoverable pair and consumes the ZIP to avoid duplicated Memory Stick use.
This validates package structure; it does not make unsigned native code safe.

The first directory is Git-hosted and uses downloadable `.rscore.zip` release
assets. Network downloading on the PSP can come later; it adds TLS, parser,
interrupted-download, and memory risks without helping core emulation.

## Next system candidates

These are research candidates, not downloadable RetroShell cores. Each needs a
new frontend system definition, extension rules, artwork, a pinned source
import, and PSP-1000 qualification before appearing in the directory.

| Priority | System | Upstream candidate | Why it is worth evaluating |
| --- | --- | --- | --- |
| 1 | Neo Geo Pocket / Color | RACE | The upstream project is itself derived from a PSP port and includes PSP-oriented MIPS assembly. |
| 2 | Atari 2600 | Stella 2014 | A deliberately older Stella libretro branch with a relatively small system target. |
| 3 | Atari Lynx | Handy | Mature single-system libretro core with a compact source tree. |
| 4 | WonderSwan / Color | Beetle WonderSwan | Existing single-system libretro integration; memory and rotation controls need measurement. |
| 5 | SNES alternate | Snes9x 2002 | Lower-era SNES engine worth measuring, but its advertised optimizations are ARM-specific and cannot be assumed to help Allegrex. |

Game & Watch, MSX, Atari 7800/8-bit, ColecoVision, and curated arcade remain a
later wave. PS1 should continue to use the PSP's native POPS route; N64 stays
out of scope without a convincing PSP-1000 result.

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
- Stella 2014 source:
  https://github.com/libretro/stella2014-libretro
- Snes9x 2002 source:
  https://github.com/libretro/snes9x2002
- Handy source:
  https://github.com/libretro/libretro-handy
- Beetle WonderSwan source:
  https://github.com/libretro/beetle-wswan-libretro
- RACE source:
  https://github.com/libretro/RACE
