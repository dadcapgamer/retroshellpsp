# Real PSP all-core test

Use the candidate build for this qualification pass. Keep RetroShell open for
the entire test so every core is recorded in one `retroshell.log`.

1. Charge the PSP, start RetroShell, and open Settings.
2. Set **In-game CPU clock** to **333 MHz**, **Show FPS** to **On**, and
   **Auto-save** to **On**.
3. For each game, open **Options -> Per-game Settings** to choose a core.
4. Let each core run for at least 30 seconds after the title screen. Play a
   demanding section rather than leaving a static menu open.
5. Open the in-game menu with **L + R + Select**, save state, load it, then
   choose **Exit**.
6. Test these cores without closing RetroShell:

   - GB/GBC: Gambatte
   - GBA: gpSP
   - NES: QuickNES
   - SNES: Snes9x 2005
   - Genesis: PicoDrive
   - Master System: SMS Plus GX

   For gpSP and SMS Plus GX, continue playing through at least one automatic
   SRAM flush (the log line begins `save: live SRAM flush`). Listen for a
   click, crunch, or slowdown around that event.

Gearboy, TGB Dual, FCEUmm, Snes9x 2005 Plus, and Snes9x 2010 are archived and
must not appear as selectable cores. Their source and provenance remain in the
repository for future investigation, but they are not part of this test.

After all cores have run, use the PSP Home button to exit RetroShell cleanly.
Reconnect USB and copy `RETROSHELL/retroshell.log` back to the project. Grade
the combined log with:

```sh
python3 tools/qualify_psp_matrix_log.py /path/to/retroshell.log
```

PPSSPP results are a correctness screen. The measurements in this real-PSP log
decide performance qualification.

For the background-SRAM focused pass, it is sufficient to retest Gambatte,
gpSP, PicoDrive, and SMS Plus GX. After the log reports
`save: async SRAM snapshot queued`, keep playing for several seconds and
confirm that gameplay and sound do not pause. Open the in-game menu afterward
so the joined `save: async SRAM flush ok` result is recorded. On the Master
System game, open **Options -> Per-game Settings** and explicitly choose
**SMS Plus GX**; PicoDrive is otherwise the higher-priority default.

## beta.15 focused pass (2026-09-08)

This build changed the shared libretro shim, which every PRX core compiles in,
so the whole matrix is a regression surface. Work down this list in order —
each step answers one open question, and steps 1-2 are the ones that decide
whether the build is sound at all.

Confirm the first log line before anything else:

```
[I] build: RetroShell 1.0.0-beta.15 (... UTC), core API v4
```

If it does not say beta.15 the EBOOT and cores are mismatched; reinstall.

### 1. Regression sweep — does the shim change break anything?

Launch one game per system, play ~30 seconds, save and load state, exit.
**Super Nintendo matters most**: Snes9x 2005 uses the shim's
`RS_PSP_NATIVE_PIXELS` path, which is the only video path that was changed but
never exercised at runtime (no SNES ROM in the tree). Watch specifically for
missing or striped output on a pseudo-hires screen such as a Secret of Mana
dialogue box.

FrogGBA's `RS_PSP_NATIVE_BGR555` path is likewise untested at runtime.

### 2. Master System mode change — the actual defect this build fixes

Needs a game that switches video mode mid-play. Upstream's own comment names
Micro Machines as a 224/240-line title; any SMS game that changes resolution
between menu and gameplay works.

Before: every frame after the switch was dropped and logged, one unbuffered
Memory Stick write per frame. Expect stutter, audio breakup, or frozen video
at the moment the mode changes.

After: the switch should be invisible. Confirm the log contains **no**
`exceeds frame buffer` or `rejected ... frame geometry` lines.

### 3. Heap reservation — is 4 MB more than the frontend needs?

Order matters. **Browse the full ROM library first** (scroll every system, let
the scan finish) and only then launch a game, otherwise the peak will not
include the game index and will read far too low.

```
heap: after boot  — ... peak N KB
heap: core launch — ... peak N KB
```

Compare `peak` against the 4096 KB reserved by `PSP_HEAP_SIZE_KB` in
`src/main.cpp`. Whatever is unused there can be returned to the arena, which
raises the ROM cache for every core on every model. A one-ROM PPSSPP library
peaked at 324 KB, but a large real library is the case that matters.

### 4. GBA ROM cache — how much paging is left?

Run the 32 MiB hack, then the trimmed 22 MiB copy, and compare:

```
[gpSP]: rom cache N MB, page faults X, jit rom flushes Y
```

Page faults are Memory Stick reads taken mid-frame; lower is better. A high
flush count would mean the 2 MiB dynarec cache is also a limit, which the
PPSSPP runs did not suggest.

Then select FrogGBA for the same game (Options -> Per-game Settings; needs a
16 KB `RETROSHELL/system/gba_bios.bin`) and judge it subjectively against
gpSP. FrogGBA owns the whole machine and should hold roughly 16 MiB against
gpSP's ~9 MiB. Expect improvement, not a cure: neither fits a 22 MiB ROM on a
32 MB PSP. Note that it exits to the XMB and keeps its own saves.

### 5. Save integrity

Confirm no `refusing oversized SRAM` errors for cartridges without battery
backup, and that a game with a real save chip still writes and restores.

Grade the combined log with `python3 tools/qualify_psp_matrix_log.py`.
