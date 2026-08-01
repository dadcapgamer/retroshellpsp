# Real PSP all-core test

Use the candidate build for this qualification pass. Keep RetroShell open for
the entire test so every core is recorded in one `retroshell.log`.

1. Charge the PSP, start RetroShell, and open Settings.
2. Set **In-game CPU clock** to **333 MHz**, **Show FPS** to **On**,
   **Auto-save** to **On**, and **PSP-1000 Safe Mode** to **Off**. Safe Mode
   remains off only because gpSP and SMS Plus GX are still candidates awaiting
   this hardware qualification.
3. For each game, highlight it and press **Square** to choose a core.
4. Let each core run for at least 30 seconds after the title screen. Play a
   demanding section rather than leaving a static menu open.
5. Open the in-game menu with **L + R + Select**, save state, load it, then
   choose **Exit game**.
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
System game, press **Square** in the library and explicitly choose **SMS Plus
GX**; PicoDrive is otherwise the higher-priority default.
