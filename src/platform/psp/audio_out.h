/** PSP audio output: one reserved hardware channel fed by a dedicated
 * thread from a lock-free ring buffer.
 *
 * This is the single audio path for the whole application — every emulator
 * core pushes here through RSHostAPI (with rate conversion), and UI sounds
 * will mix into the same stream. Output is fixed 44100 Hz stereo s16.
 */
#pragma once

#include "rs_common.h"

namespace rs::audio {

constexpr u32 OUTPUT_RATE = 44100;
/* PSP hardware accepts power-of-two blocks. A 256-frame block limits an
 * unavoidable starvation gap to 5.8 ms and lets recovery react twice as
 * quickly; the blocking audio thread still sleeps in hardware between
 * submissions, so the extra wakeups do not busy-spin the CPU. */
constexpr u32 OUTPUT_BLOCK_FRAMES = 256;

bool init();
void shutdown();

enum class UiSound { Move = 0, Confirm, Back };
/* Short synthesized UI cues. They are mixed by the existing audio thread,
 * require no heap allocation/assets, and remain audible while core audio is
 * intentionally paused in frontend menus. */
void playUiSound(UiSound sound);

/* Producer side (main thread). Frames are stereo pairs at `srcRate`;
 * linear resampling to 44100 happens on the way into the ring. */
void setSourceRate(u32 srcRate);
void push(const s16* stereo, u32 frames);

/* Drop whatever is buffered (state load, core switch). */
void clear();
/* Suppress starvation accounting while emulation is intentionally paused.
 * Resume only after clear() so stale gameplay audio is never replayed. */
void setPaused(bool paused);
bool isPaused();

/* Ring occupancy in output frames — lets the session pace emulation. */
u32 buffered();
u32 capacity();

/* Monotonic session telemetry used by the PSP-1000 release gate. */
u32 underruns();
u32 droppedFrames();
void resetTelemetry();

}  // namespace rs::audio
