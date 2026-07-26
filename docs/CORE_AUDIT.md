# PSP-first core audit and release gate

This file records decisions separately from claims that require real hardware.
PPSSPP results may find correctness regressions, but they never approve speed,
audio stability, or PSP-1000 memory safety.

## Current delivery decisions

| System | Core | Delivery | Decision and remaining evidence |
|---|---|---|---|
| GB/GBC | Gambatte | Production default | Keep. Pinned and reproducible; hardware compatibility/performance gate remains required for a release. |
| GB/GBC | Gearboy | Candidate only | Optimize/evaluate. Retain only if it passes every gate and fixes a documented Gambatte failure. |
| GB/GBC | TGB Dual | Candidate only | Evaluate/remove. Lower priority; retain only for a documented compatibility advantage. |
| NES | FCEUmm | Production default | Keep pending the PSP-1000 workload and 20-cycle gate. |
| SNES | Snes9x 2005 | Production default | Keep for baseline titles; it met PPSSPP's frame budget but failed the tested modern SA-1 ROM. |
| SNES | Snes9x 2010 | Candidate only | Compatibility alternate. The PSP-pruned build renders the tested SA-1 ROM and passes save/load/relaunch memory checks, but PPSSPP timing and underruns require real-hardware evaluation before promotion. |
| MD/SMS/GG | PicoDrive | Production default | Keep. The opaque archive was replaced by pinned source and `LOW_MEMORY=1`; PSP-1000 measurements remain required. |
| SMS/GG | SMS Plus GX | Not integrated | Evaluate as an alternate only after the shared gates pass. |
| GBA | gpSP | Blocked | Do not integrate until VFS streaming, fuzzing, and memory tests pass. |
| PC Engine | Lightweight candidate TBD | Blocked | Do not integrate until the same shared infrastructure passes. |

“Production default” describes package selection, not final hardware approval.
The release is still blocked until both hardware columns below contain measured
results.

## Mandatory hardware matrix

For every production core, record legal homebrew/test-ROM hashes and:

| Core | PSP-1000 (32 MB) | Later PSP (64 MB) | 20 launch/exits | Persistent underruns | Save/SRAM integrity |
|---|---|---|---|---|---|
| Gambatte | NOT RUN | NOT RUN | NOT RUN | NOT RUN | NOT RUN |
| FCEUmm | NOT RUN | NOT RUN | NOT RUN | NOT RUN | NOT RUN |
| Snes9x 2005 | NOT RUN | NOT RUN | NOT RUN | NOT RUN | NOT RUN |
| PicoDrive | NOT RUN | NOT RUN | NOT RUN | NOT RUN | NOT RUN |

A core is disqualified by any PSP-1000 allocation failure, save corruption,
failed 20-cycle run, persistent underrun, or inability to sustain native speed
at 333 MHz. The logged p95 emulation time must remain within the native frame
budget with bounded arena high-water.

## Automated checks

- `./build.sh test` validates manifest schema, deterministic core selection,
  test-only/production agreement, full upstream commits, retained licenses,
  vendored tree hashes, and the PRX-only build policy.
- `./build.sh release` performs a PSP build, checks every PRX against the
  lockfile, and emits a deterministic ZIP containing only production cores,
  manifests, licenses, provenance, and corresponding-source notices.
- Parsers enforce explicit budgets for JSON/cache/images/ZIP/state/SRAM data;
  ZIP extraction additionally limits entry count, encryption, unsupported
  methods, expanded size, and decompression ratio.
- Runtime telemetry logs p95 frame time, arena use/high-water, allocation
  failures, and audio underruns/dropped frames.
- A debug build with `-DRS_FAILURE_INJECTION=ON` accepts the per-game
  `injectFailure` option values `missing_prx`, `bad_api`, `arena_exhaustion`,
  `init_failure`, `corrupt_rom`, `rom_rejection`, and
  `framebuffer_allocation`; every case exercises the normal staged unwind.

## PPSSPP regression record

2026-07-25, PPSSPP 1.20.4, PSP-1000 model, software rendering:

- A clean production boot registered only Gambatte, FCEUmm, Snes9x 2005, and
  PicoDrive; test-only candidates and the dummy core were excluded.
- The scripted PicoDrive run navigated the frontend, launched a user-supplied
  Mega Drive game, saved and loaded a 679,178-byte state, exited through the
  frontend menu, restored and rescanned the frontend, then relaunched the game.
- Both launches sustained 60 fps at the emulated 333 MHz clock. Observed p95
  emulation time was 6.2–14.4 ms, below the 16.67 ms frame budget.
- The first core session peaked at 2,106 KB of arena use during state handling,
  reported zero allocation failures, and left 1 KB live at module unload.
- Audio drops remained zero. Underruns rose around startup, screenshot, and
  state operations, then stopped increasing during sustained play; this is not
  accepted as hardware audio evidence.

This run covers navigation, launch, screenshots, state save/load,
return-to-menu recovery, and one core relaunch. Suspend/resume, malformed-input
coverage, repeated core switching, the full 20-cycle run, and every real
hardware gate remain outstanding.

### SNES compatibility candidate

2026-07-25, the same PPSSPP PSP-1000 configuration:

- Snes9x 2005 rendered a baseline homebrew ROM at 60 fps but produced zero
  non-black pixels for the user-supplied 4 MiB SA-1 game.
- The pinned Snes9x 2010 candidate identified the cartridge as SA-1 LoROM and
  rendered its title screen. Host-VFS loading removed the duplicate 4 MiB ROM
  image; steady arena high-water fell from 14,313 KB to 10,281 KB, leaving
  4,476 KB free.
- The automated cycle saved and loaded an 826,530-byte state, flushed the
  131,072-byte SRAM, unloaded with zero live core bytes and zero allocation
  failures, restored the frontend, and relaunched the game.
- PPSSPP reported roughly 24–34 ms p95 emulation time and persistent audio
  underruns for this SA-1 workload. It therefore remains a test-only
  compatibility candidate; PPSSPP is not a performance authority, and the
  real PSP-1000 gate has not passed.

## Required lab runs before tagging a release

1. Run PPSSPP with the PSP-1000 memory model through navigation, malformed
   inputs, save/load, screenshots, suspend/resume, repeated core switches, and
   return-to-menu recovery.
2. Run parser fuzz targets under ASan/UBSan when the host fuzz harness is
   available; retain every crash as a regression corpus.
3. Execute the matrix above on a PSP-1000 and one 64 MB model using the same
   test hashes and 333 MHz clock.
4. Attach logs containing frame percentiles, arena high-water, system free
   memory, underruns, boot time, and all 20 launch/exit cycles.
5. Do not tag while a critical/high finding or any `NOT RUN` production gate
   remains.
