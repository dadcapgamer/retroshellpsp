/** Battery status, wall clock, and CPU frequency control. */
#pragma once

namespace rs::power {

/* 0..100, or -1 when unknown (e.g. PSP without a battery in PPSSPP). */
int  batteryPercent();
bool batteryCharging();

/* Local time of day. */
void clockNow(int& hour, int& minute);

/* 222 for menus (battery-friendly), 333 for demanding cores. */
void setCpuMhz(int mhz);

}  // namespace rs::power
