#include "platform/psp/power.h"

#include <psppower.h>
#include <psprtc.h>

namespace rs::power {

int batteryPercent() {
    if (!scePowerIsBatteryExist()) return -1;
    const int pct = scePowerGetBatteryLifePercent();
    return (pct < 0 || pct > 100) ? -1 : pct;
}

bool batteryCharging() { return scePowerIsBatteryCharging() > 0; }

void clockNow(int& hour, int& minute) {
    ScePspDateTime t{};
    sceRtcGetCurrentClockLocalTime(&t);
    hour   = t.hour;
    minute = t.minute;
}

void setCpuMhz(int mhz) {
    scePowerSetClockFrequency(mhz, mhz, mhz / 2);
}

int cpuMhz() { return scePowerGetCpuClockFrequency(); }

}  // namespace rs::power
