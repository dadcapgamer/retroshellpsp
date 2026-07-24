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

bool init();
void shutdown();

/* Producer side (main thread). Frames are stereo pairs at `srcRate`;
 * linear resampling to 44100 happens on the way into the ring. */
void setSourceRate(u32 srcRate);
void push(const s16* stereo, u32 frames);

/* Drop whatever is buffered (state load, core switch). */
void clear();

/* Ring occupancy in output frames — lets the session pace emulation. */
u32 buffered();

}  // namespace rs::audio
