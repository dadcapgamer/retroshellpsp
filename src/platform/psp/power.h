/** Battery status, wall clock, and CPU frequency control. */
#pragma once

#include "rs_common.h"

namespace rs::power {

enum Event : u32 {
    EVENT_NONE       = 0,
    EVENT_SUSPENDING = 1u << 0,
    EVENT_RESUMED    = 1u << 1,
};

/* Called by the PSP callback thread. The application consumes these flags on
 * its own thread so no renderer, core, or audio lifecycle work happens inside
 * a kernel callback. */
void notifyCallback(int pspPowerFlags);
u32  consumeEvents();

/* 0..100, or -1 when unknown (e.g. PSP without a battery in PPSSPP). */
int  batteryPercent();
bool batteryCharging();

/* Local time of day. */
void clockNow(int& hour, int& minute);
/* Local wall-clock time packed as YYYYMMDDHHMM. The 12-digit value remains
 * exactly representable in JSON's numeric format and is stable across boots. */
u64  localTimestamp();

/* 222 for menus (battery-friendly), 333 for demanding cores. */
void setCpuMhz(int mhz);
int  cpuMhz();            /* actual current CPU clock, for diagnostics */

}  // namespace rs::power
