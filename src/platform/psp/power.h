/** Battery status, wall clock, and CPU frequency control. */
#pragma once

#include "rs_common.h"

namespace rs::power {

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
