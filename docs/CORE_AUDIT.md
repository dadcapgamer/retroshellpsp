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
| NES | QuickNES | Production default | PPSSPP model-0 lifecycle gate passed; real PSP-1000 workload and 20-cycle gate remain. |
| NES | FCEUmm | Candidate only | Demoted after PPSSPP ran at 80% median speed with persistent audio starvation and stopped responding after state save. Retained only for compatibility comparison; skip state load on hardware. |
| SNES | Snes9x 2005 | Production default | PSP-native BGR555, balanced audio, recovery frames, and a PRX-safe subset of the Snes9xTYL compiler profile. Keep as the only hardware-safe SNES baseline; demanding real-hardware workloads still require retest. |
| SNES | Snes9x 2010 | **Removed** | Repeatedly caused a native freeze and power-off immediately after loading on real PSP-1000 hardware. It fails the mandatory safety gate and is no longer built or packaged. |
| MD/SMS/GG | PicoDrive | Production default | Keep. Pinned-source `LOW_MEMORY=1` build with native BGR565 and 32 kHz audio. Hardware rejected the shallow recovery experiment, so the proven six-frame profile is restored. |
| SMS/GG | SMS Plus GX | Candidate only | PPSSPP lifecycle gate passed; evaluate as a PicoDrive compatibility alternate on hardware. |
| GBA | gpSP | Candidate only | Pinned PSP/MIPS dynarec build with VFS v2 paging and an 8 MiB ROM cache. The first PSP-1000 run passed speed, frame-time, memory, allocation, and state tests but failed the persistent-audio-underrun gate. A 32.8 kHz PSP mixer default and emergency-only automatic frame skipping now await retest. Keep hidden in PSP-1000 Safe Mode until audio, SRAM, and 20-cycle tests pass. |
| PC Engine | Beetle PCE Fast | Candidate only | Pinned HuCard build now uses Allegrex O3, 32 kHz audio, native BGR565, and balanced recovery frames. It passes PPSSPP model-0 but remains hidden in PSP-1000 Safe Mode until the new build passes real-hardware performance/audio and 20-cycle tests. |

“Production default” describes package selection, not final hardware approval.
The release is still blocked until both hardware columns below contain measured
results.

## Mandatory hardware matrix

For every production core, record legal homebrew/test-ROM hashes and:

| Core | PSP-1000 (32 MB) | Later PSP (64 MB) | 20 launch/exits | Persistent underruns | Save/SRAM integrity |
|---|---|---|---|---|---|
| Gambatte | 99.5% median, 12.634 ms p95, 1,505 KB peak; audio gate failed | NOT RUN | NOT RUN | FAIL: intermittent gameplay bursts | PASS |
| QuickNES | PASS: 100% median, 8.236 ms p95, 968 KB peak | NOT RUN | NOT RUN | PASS | PASS |
| Snes9x 2005 | FAIL on Secret of Mana: 80% median, 20.808 ms p95, 12,395 KB peak | NOT RUN | NOT RUN | FAIL | PASS |
| PicoDrive | 100% median, 10.581 ms p95, 633 KB peak on SMS; audio gate failed; MD workload not recorded | NOT RUN | NOT RUN | FAIL: late sustained burst | PASS |

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
- gpSP prefers host VFS loading, so uncompressed GBA ROMs are not duplicated
  in the core arena. Compressed GBA entries are rejected with an actionable
  error until bounded temporary extraction is implemented.
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

### Full candidate matrix

2026-07-26, PPSSPP PSP-1000 model, one isolated PRX per run:

- Gambatte, Gearboy, gpSP, PicoDrive, QuickNES, SMS Plus GX, Snes9x 2005,
  Snes9x 2005 Plus, and TGB Dual completed launch, save/load, exit, frontend
  recovery, relaunch, screenshots, and clean shutdown with no allocation
  failures.
- QuickNES sustained 100% median speed with 4.696 ms median p95 and 940 KB
  peak arena use on the NES workload. Its uninitialized turbo-input state was
  fixed and the patched core passed a second complete run.
- FCEUmm sustained only 80% median speed with 17.851 ms median p95 and
  persistent PPSSPP audio starvation. It wrote a 13,804-byte state, then the
  session stopped responding before load/exit. It was demoted to candidate;
  QuickNES is now the deterministic NES default.
- The runner now accepts only valid video output, treats bounded black
  boot/transition samples as telemetry, tolerates one-page model-0 arena
  alignment variation, and preserves the ROM identity before removable media
  can be disconnected.

### PC Engine candidate

2026-07-28, PPSSPP 1.20.4, PSP-1000 model:

- Beetle PCE Fast launched Victory Run twice, saved and loaded an 80,586-byte
  state, returned to the frontend, and exited cleanly.
- The run sustained 100% median speed with an 11.374 ms median p95, 1,964 KB
  peak arena use, zero allocation failures, valid changing video, and zero
  audio drops.
- CHD, LZMA, and Zstd are excluded from the initial candidate. PC Engine CD
  support is not advertised or qualified.
- This is a PPSSPP correctness result only; the candidate remains test-only
  and hidden by PSP-1000 Safe Mode pending real-hardware acceptance.

## 2026-07-26 all-core PSP-1000 hardware pass

One uninterrupted real PSP-1000 session switched through nine cores and ended
with a clean application exit. Every launched PRX unloaded, the frontend
recovered after every switch, all attempted state saves and loads succeeded,
all SRAM flushes succeeded, and every core recorded zero allocation failures.
FCEUmm was not launched; its PPSSPP state-path stall remains the deciding
evidence against shipping it.

| Core | Hardware result | Decision |
|---|---|---|
| Gambatte | 99.5% median speed, 12.634 ms p95, 1,505 KB peak; intermittent underrun bursts, no drops | **Keep/optimize audio.** Still the best measured GB/GBC default. |
| Gearboy | 65% median speed, 27.568 ms p95, 4,235 KB peak; continuous starvation | **Remove from PSP-1000 packages.** |
| TGB Dual | 100% median speed, 11.695 ms p95, 1,476 KB peak, but only 12 samples and video stopped uploading after sequence 2 | **Fix video integration or remove.** Not qualified by this run. |
| gpSP | 100% median speed, 11.966 ms p95, 8,892 KB peak; bounded but recurring underrun bursts | **Keep/optimize audio.** GBA candidate remains viable. |
| QuickNES | 100% median speed, 8.236 ms p95, 968 KB peak; final steady-tail underrun delta 2 | **Keep.** Best complete result and NES default. |
| Snes9x 2005 | 80% median speed, 20.808 ms p95, 12,395 KB peak on Secret of Mana; persistent starvation | **Optimize or replace for stress titles.** Save safety passed. |
| Snes9x 2005 Plus | 56% median speed, 34.930 ms p95, 14,437 KB peak; continuous starvation | **Remove from PSP-1000 packages.** |
| PicoDrive | 100% median speed, 10.581 ms p95, 633 KB peak on Master System; late underrun burst | **Keep/optimize audio.** Genesis still needs a measured run. |
| SMS Plus GX | 100% median speed, 11.045 ms p95, 1,471 KB peak; late underrun burst | **Retest after shared audio work.** PicoDrive is faster and lighter in this workload. |
| FCEUmm | Not launched on hardware; PPSSPP measured 80% and stalled after state save | **Remove from qualification packages.** |

This is one hardware pass, not the required 20-cycle acceptance test. No
64 MB PSP result has been recorded. The next build should focus on shared
audio scheduling, remove the clear PSP-1000 losers, repair or retire TGB
Dual's static-output path, and evaluate a lighter SNES candidate before the
20-cycle production-only run.

### Removed SNES compatibility candidate

2026-07-25, the same PPSSPP PSP-1000 configuration:

- Snes9x 2005 rendered a baseline homebrew ROM at 60 fps but produced zero
  non-black pixels for the user-supplied 4 MiB SA-1 game.
- The pinned Snes9x 2010 candidate identified the cartridge as SA-1 LoROM and
  rendered its title screen in PPSSPP. Host-VFS loading removed the duplicate
  4 MiB ROM image; steady arena high-water fell from 14,313 KB to 10,281 KB,
  leaving 4,476 KB free.
- The automated cycle saved and loaded an 826,530-byte state, flushed the
  131,072-byte SRAM, unloaded with zero live core bytes and zero allocation
  failures, restored the frontend, and relaunched the game.
- PPSSPP reported roughly 24–34 ms p95 emulation time and persistent audio
  underruns for this SA-1 workload. On real PSP-1000 hardware it repeatedly
  froze and powered off the device during launch. It is disqualified and
  removed; PPSSPP success cannot override the hardware safety gate.

## PSP-1000 GBA candidate record

2026-07-26, real PSP-1000, gpSP, user-supplied Pokemon Emerald workload:

- The core loaded and streamed the ROM with 3,889 KB free after launch. Arena
  high-water remained bounded at 8,764 KB and there were no allocation
  failures.
- The 154-sample qualifier recorded 100.0% median speed and 12.306 ms median
  p95 core time, within the 16.74 ms GBA frame budget. A 425,984-byte state
  saved and loaded successfully.
- The run failed only the audio gate: underruns increased by 331, primarily
  during boot and scene transitions. The core still rendered every frame
  because its own frontend-aware frame skipping was disabled.
- The next build supplies PSP-specific gpSP defaults of 32,768 Hz internal
  audio and automatic frame skipping only when the audio callback reports
  imminent starvation. User per-game settings retain priority.
- HOME was used to leave the application while the game was active. The log
  lacked SRAM, PRX-unload, and core-heap shutdown records, revealing that the
  application-exit path destroyed the scene without staged teardown. The next
  build explicitly flushes SRAM and unloads the core before shutting down
  application services.

2026-07-26 follow-up, same hardware and workload:

- The PSP defaults were confirmed active at 32,768 Hz with automatic
  frameskip. Median speed remained 100.0%, median p95 core time improved
  slightly to 12.144 ms, arena high-water remained 8,764 KB, and allocation
  failures remained zero.
- State save/load, two 131,072-byte SRAM writes, PRX unload, core-heap
  telemetry, frontend restoration, and clean application exit all completed.
- Underruns fell from 331 to 265 but still failed the gate. Most accumulated
  during one sustained heavy scene at 43–44 fps, where rendered frames cost
  about 21 ms but skipped frames cost about 12 ms.
- gpSP's automatic mode skipped only after the ring was nearly empty and
  ignored RetroShell's earlier standard video-disable requests. The next
  candidate honours that libretro signal so recovery bursts suppress scanline
  rendering while preserving logic and audio.

2026-07-26 recovery-signal follow-up, real PSP-1000:

- Pokemon Emerald and Kururin Paradise both sustained approximately native
  speed during ordinary play. gpSP now honoured frontend skip requests:
  skipped-frame cost fell to roughly 3–5 ms in ordinary scenes, and Emerald's
  demanding in-game save improved from 43–44 fps to 51–56 fps.
- Kururin's heavier scenes sustained 59–60 fps by rendering only the newest
  presentation frame while running intervening logic/audio frames without
  scanline rendering.
- The remaining repeatable audio bursts followed each synchronous
  `save: sram ok` Memory Stick write (about 10–11 hardware audio blocks per
  128 KiB atomic save). The next candidate pre-buffers the audio ring with
  bounded logic-only frames before live SRAM writes and logs write duration,
  buffer change, and the exact underrun delta.

2026-07-26 live-SRAM-buffer follow-up, real PSP-1000:

- Each 128 KiB atomic Memory Stick write took 151–153 ms. Eight bounded
  logic/audio-only frames reduced the write-correlated underrun cost from
  roughly 10–11 blocks to 1 immediately and 2–3 across the surrounding
  sample, without dropped audio frames.
- Ordinary play sustained 59–60 fps, median speed was 100%, median p95 core
  time was 12.080 ms, arena high-water was 8,892 KB, and allocation failures
  remained zero. State, SRAM, unload, frontend restoration, and clean exit
  all passed.
- Emerald's own emulated flash-save workload remains a bounded stress event
  at 53–55 fps; it no longer destabilises play after the operation completes.
  The underrun counter stayed flat throughout the final steady section.
- gpSP is frozen as the GBA candidate baseline. It passes the single-device
  performance/memory/save gate but remains candidate-only pending automated
  PPSSPP regression coverage, broader legal-ROM compatibility, 20 launch/exit
  cycles, and later-model PSP measurements.

## 2026-07-26 active-core PPSSPP regression

- Gambatte, gpSP, QuickNES, Snes9x 2005, PicoDrive, and SMS Plus GX all passed
  the PSP-1000-memory autopilot lifecycle: launch, save/load, frontend return,
  relaunch, screenshots, clean exit, valid video, and zero allocation
  failures.
- The initial run exposed audio-ring overflow after live SRAM priming in gpSP
  (734 dropped output frames) and SMS Plus GX (515). Live-save priming now
  uses the same bounded recovery target as startup, and recovery batches stop
  as soon as that target is reached.
- Focused gpSP and SMS Plus GX reruns both sustained 100% median speed and
  reported zero dropped audio frames. `no_audio_drops` is now a mandatory
  automation check rather than telemetry-only data.
- The active hardware bundle contains four production cores and two
  candidates. Gearboy, TGB Dual, FCEUmm, Snes9x 2005 Plus, and Snes9x 2010
  remain archived and are blocked even if stale PRX files are present.

## 2026-07-26 six-core hardware follow-up

- Five cores were recorded on the PSP-1000; SMS Plus GX was not selected.
  QuickNES passed at 100% median speed, 8.311 ms median p95, zero allocation
  failures, zero underruns, and zero dropped frames.
- Gambatte, gpSP, and PicoDrive met the speed, memory, video, and zero-drop
  gates. Their audio failures were tied to synchronous atomic SRAM writes:
  the real Memory Stick took 145–154 ms, completely draining the ring while
  the main emulation thread was blocked.
- Periodic SRAM now copies at most 1 MiB into a bounded core-arena snapshot
  and writes it on a low-priority joined worker. The next interval, in-game
  menu, or teardown joins and reports the result before the snapshot is freed
  or the core is unloaded. Focused gpSP and SMS Plus PPSSPP lifecycles passed
  with zero drops after this change.
- Snes9x 2005 remains a separate performance failure on Secret of Mana:
  82% median speed, 20.436 ms median p95, and sustained underruns in demanding
  gameplay. It is memory-safe and its save/load lifecycle completed, but it
  is not ready to qualify as the final SNES default.

## 2026-07-27 Snes9x 2005 optimization candidate

- The first optimization candidate incorrectly suppressed every recovery
  frame until the audio queue reached its high watermark. On real PSP-1000
  hardware the core could maintain about 1,400 queued samples but could not
  reach the 3,584-sample exit threshold, permanently freezing video while
  music and emulation continued. Recovery now skips only obsolete
  intermediate images and always presents the final frame in each bounded
  four-frame batch. It still stops early when the safe target is reached.
- The isolated PRX now restores the upstream PSP release target's Allegrex,
  non-PIC, section-GC, `LAGFIX`, and `NDEBUG` settings. The production core
  shrank to 999,270 bytes while retaining the frontend-owned GU path and
  PSP-native BGR555 framebuffer.
- A PSP-only balanced audio mode mixes at 22,050 Hz instead of 32,040 Hz,
  reducing old-mixer iterations by about 31%. Stereo, interpolation, and echo
  stay enabled; the shared frontend resampler continues to feed PSP audio at
  44,100 Hz.
- Secret of Mana and Super Mario World both pass the PPSSPP PSP-1000-memory
  lifecycle, including two launches, state save/load, valid changing video,
  frontend recovery, clean exit, zero dropped output frames, and zero
  allocation failures. Their PPSSPP median speeds were 100%; PPSSPP is not
  accepted as native performance evidence.
- This build is the next real-hardware candidate. It must improve Secret of
  Mana's previous 82% median speed and 20.436 ms median p95 without audible
  regression before the SNES core can pass the production performance gate.

2026-07-27 real PSP-1000 follow-up:

- The freeze guard restored visual progress, but the candidate still failed:
  80% median speed, 28.235 ms median p95, 7,938 new underruns, and another 551
  underruns in the final ten samples. Transparency-heavy rendered frames
  reached roughly 29-35 ms while skipped frames remained near 12 ms.
- Four-frame recovery batches took just over four 16.7 ms display intervals,
  forcing the system down to roughly 48 emulated fps. Recovery now uses five
  frames so its one presentation cost is amortized across the five-vblank
  window; the worst-case presentation rate remains about 12 fps while logic
  and audio have enough budget to approach 60 Hz.
- A photographed dialogue box exposed partially stale texture contents. The
  zero-copy shim had omitted the data-cache writeback performed by upstream's
  native PSP path before the GE reads `GFX.Screen`. The shim now writes the
  rendered range back on every presented frame. This is a correctness fix,
  not an optional performance tradeoff.

2026-07-27 cache-writeback hardware follow-up:

- The installed hashes matched the intended EBOOT and PRX. Cache writeback
  did not fix the dialogue appearance because the captured framebuffer itself
  contained alternating scene/window columns; it was not stale GE texture
  data.
- The core switches to 512-wide output for Secret of Mana's SNES pseudo-hires
  transparency. Scaling those alternating main/subscreen pixels directly to
  the PSP's 480-pixel display creates vertical moire and a ghosted textbox.
  The shim now averages adjacent BGR555 pixel pairs into a 256-wide resolved
  frame. Normal 256-wide frames remain zero-copy.
- Five-frame recovery improved the median hardware speed from 80% to 83% and
  reduced total underrun rate, but still failed at 30.710 ms median p95 and
  5,766 new underruns. It produced 50-55 emulation frames and 10-11
  presentations per second in the steady tail. Six-frame recovery retains
  approximately ten presentations while budgeting the missing logic/audio
  frames needed to approach 60 Hz.

## 2026-07-28 PicoDrive, PCE Fast, and SNES optimization pass

- This pass initially tested a balanced three-frame recovery cap and
  2,560-frame exit watermark for PicoDrive and PCE Fast. The follow-up
  hardware results below determine their final profiles.
- PCE Fast now mixes at 32 kHz on PSP, honours standard frontend video-disable
  requests, generates native PSP BGR565 pixels, and exposes that surface
  directly to the frontend shim. This removes the per-pixel red/blue swap and
  redundant shim framebuffer. Its HuCard-only candidate status is unchanged.
- Snes9x 2005 now applies the PRX-safe portions of Snes9xTYL's Allegrex
  compiler profile: predictive commoning, register renaming, aggressive
  inlining, and PSP-oriented function/loop alignment. TYL's GU renderer,
  Media Engine ownership, and whole-program mode remain excluded because
  they conflict with RetroShell's host-owned services and unloadable PRXs.
- PPSSPP model-0 autopilot passed launch, state save/load, exit, frontend
  recovery, relaunch, valid video, zero allocation failures, and zero audio
  drops for Foxy Land/PicoDrive, Victory Run/PCE Fast, and Secret of
  Mana/Snes9x 2005. Median speed was 100% for all three; median p95 times were
  7.084 ms, 8.720 ms, and 5.583 ms respectively. These are correctness
  regressions only and do not replace the pending real PSP-1000 measurements.

2026-07-28 real PSP-1000 follow-up:

- The three-frame PicoDrive experiment failed. Foxy Land fell to 75% median
  speed with 3,951 new underruns; Castle of Illusion fell to 80% with 2,874.
  PicoDrive is restored to the prior six-frame/3,584-frame profile that had
  sustained 100% in both workloads.
- PCE Fast improved from 75% to 99% median speed and reduced Victory Run's
  underruns from 4,225 to 139. Its native-video/audio work is retained, while
  recovery increases from three to four frames to replenish rather than only
  maintain the queue.
- Super Mario World sustained 100% with no new underruns. Secret of Mana
  improved from roughly 83% to 85%, with median p95 falling to 29.577 ms and
  new underruns falling from 5,766 to 2,020, but recovery still presented
  only 9–10 images per second. The clean US ROM hash matches Snes9xTYL's
  SNESAdvance database entry, so the next build applies its three exact
  byte-validated idle-loop substitutions.

2026-07-28 Secret of Mana speed-hack and PCE recovery follow-up:

- Foxy Land returned to 100% speed with zero underruns after PicoDrive's
  six-frame recovery profile was restored.
- The clean-US Secret of Mana substitutions were effective. Gameplay
  sustained approximately 98–102%, rendered-frame cost fell from about
  30 ms to 23–25 ms in comparable sections, and the audio queue remained
  stable. The remaining perceived combat delay is no longer explained by
  emulation slowdown; Secret of Mana's stamina/accuracy mechanics and the
  frontend's visual cadence must be evaluated separately.
- Victory Run's game logic and audio continued at 99–100% with zero
  underruns, but uploaded frame sequences periodically stopped for several
  seconds. Recovery had reached its exit watermark on an intermediate
  skipped frame and broken out before the batch's designated presentation
  frame. The scheduler now shortens such a batch to one guaranteed rendered
  final frame instead of exiting immediately.
- Secret of Mana dialogue remained striped. The next core ports Snes9xTYL's
  stricter pseudo-hires predicate and corrected main/sub-screen layer masks,
  addressing the source colour-math path rather than painting a hard-coded
  rectangle over game output.

2026-07-28 PSP-1000 `tyl3` hardware follow-up and `tyl4` scheduler:

- PCE Fast/Victory Run sustained 99-100% speed, 15.144 ms median p95, zero
  underruns, zero drops, and continuously advancing video. The prior
  presentation stall is fixed; its four-frame profile is retained.
- Snes9x 2005/Secret of Mana sustained 100% median game speed in both runs.
  The heavier run held a stable audio queue with one isolated underrun and
  zero drops, but its six-frame recovery batches presented only about 15
  images per second despite 25-30 ms rendered frames.
- `tyl4` gives Snes9x 2005 a dedicated four-frame recovery cap. Based on the
  measured 8-10 ms skipped and 25-30 ms rendered costs, this targets roughly
  20 presentations per second while preserving about one frame of audio
  recovery per demanding batch. PicoDrive remains on its independently
  proven six-frame profile.
- PPSSPP model 0 remains the memory, lifecycle, state, and visual-regression
  gate. Real PSP-1000 hardware remains the performance and audio authority
  because PPSSPP does not reproduce native Allegrex, GE, vblank, and Memory
  Stick timing closely enough for frame-budget qualification.

2026-07-28 PSP-1000 `tyl4` follow-up and `tyl5` candidate:

- Motion and combat animation were subjectively smoother, confirming the
  four-frame presentation cadence, but the hardware log accumulated 660
  audio underruns. `tyl5` retains four-frame recovery while deepening to six
  only below 1,536 queued samples, preventing a sustained low-buffer state
  from starving the audio thread.
- The remaining Secret of Mana dialogue stripes are not frontend scaling.
  TYL uses separate PSP GU main/sub-screen passes for high-resolution output,
  while RetroShell resolves the core's 512 columns into 256. `tyl5`'s attempt
  to select the main screen in the core changed the artifact into larger
  blue/black blocks and was reverted.
- `tyl6` keeps TYL's pseudo-hires register handling intact and moves the
  title-specific approximation to the 512-to-256 resolver. For the already
  byte-validated clean US ROM only, the resolver preserves the member of each
  pixel pair with the greater BGR555 blue component; this retains dark-blue
  fill and white glyphs instead of averaging them with the alternate screen.
  All other titles retain the normal format-correct pair average.
- The `tyl6` clean-US Secret of Mana PPSSPP model-0 run passed two launches,
  save/load, frontend return, video validation, and the 32 MB arena gate with
  no allocation failures or audio drops. Dialogue appearance, enemy response,
  and underrun stability remain native-hardware acceptance checks.

## Required lab runs before tagging a release

1. Extend the passing PSP-1000 PPSSPP lifecycle matrix with malformed-input
   and suspend/resume coverage.
2. Run parser fuzz targets under ASan/UBSan when the host fuzz harness is
   available; retain every crash as a regression corpus.
3. Execute the matrix above on a PSP-1000 and one 64 MB model using the same
   test hashes and 333 MHz clock.
4. Attach logs containing frame percentiles, arena high-water, system free
   memory, underruns, boot time, and all 20 launch/exit cycles.
5. Do not tag while a critical/high finding or any `NOT RUN` production gate
   remains.
