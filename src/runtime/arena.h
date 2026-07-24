/** The boot-time memory arena — RetroSuite's big-allocation strategy.
 *
 * At startup the frontend reserves the largest available block of the user
 * partition (leaving a small fixed newlib heap for incidental allocations).
 * Everything large — UI textures, boxart, ROM images, emulator core memory —
 * comes from here via bump allocation with stack-marker discipline:
 *
 *   boot assets → marker() → frontend caches → reset(marker) on eviction,
 *   then the launched core allocates from the same space.
 *
 * This is what makes the bi-layer launch protocol cheap: releasing every
 * frontend resource is a single pointer reset.
 */
#pragma once

#include "rs_common.h"

namespace rs::mem {

using Marker = u32;

bool   init();
void   shutdown();

void*  alloc(u32 size, u32 align = 16);
Marker marker();
void   reset(Marker m);

u32    available();
u32    totalSize();
u32    highWater();

}  // namespace rs::mem
