#include "platform/psp/power.h"

#include <psppower.h>
#include <psprtc.h>

#include <atomic>

namespace rs::power {

namespace {
std::atomic<u32> s_events{EVENT_NONE};
}

void notifyCallback(int flags) {
    u32 events = EVENT_NONE;
    if (flags & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_SUSPENDING |
                 PSP_POWER_CB_STANDBY))
        events |= EVENT_SUSPENDING;
    if (flags & (PSP_POWER_CB_RESUMING | PSP_POWER_CB_RESUME_COMPLETE))
        events |= EVENT_RESUMED;
    if (events) s_events.fetch_or(events, std::memory_order_release);
}

u32 consumeEvents() {
    return s_events.exchange(EVENT_NONE, std::memory_order_acq_rel);
}

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

u64 localTimestamp() {
    ScePspDateTime t{};
    sceRtcGetCurrentClockLocalTime(&t);
    return u64(t.year) * 100000000ull +
           u64(t.month) * 1000000ull +
           u64(t.day) * 10000ull +
           u64(t.hour) * 100ull +
           u64(t.minute);
}

void setCpuMhz(int mhz) {
    if (mhz != 222 && mhz != 266 && mhz != 333) mhz = 222;
    scePowerSetClockFrequency(mhz, mhz, mhz / 2);
}

int cpuMhz() { return scePowerGetCpuClockFrequency(); }

}  // namespace rs::power
