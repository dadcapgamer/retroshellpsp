#include "platform/psp/input_pad.h"

#include <pspkernel.h>

namespace rs::input {

namespace {
constexpr u32 DIRS[4] = {PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT,
                         PSP_CTRL_RIGHT};
constexpr int ANALOG_DEADZONE = 48;
}  // namespace

void Pad::init() {
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    m_lastPollUs = sceKernelGetSystemTimeLow();
}

void Pad::poll() {
    SceCtrlData data{};
    sceCtrlPeekBufferPositive(&data, 1);

    const u32 now = sceKernelGetSystemTimeLow();
    const float dt = float(now - m_lastPollUs) * 1e-6f;
    m_lastPollUs = now;

    u32 held = data.Buttons | m_simHeld;
    m_simHeld = 0;

    /* Fold analog into the d-pad for navigation. */
    const int ax = int(data.Lx) - 128;
    const int ay = int(data.Ly) - 128;
    if (ax < -ANALOG_DEADZONE) held |= PSP_CTRL_LEFT;
    if (ax > ANALOG_DEADZONE)  held |= PSP_CTRL_RIGHT;
    if (ay < -ANALOG_DEADZONE) held |= PSP_CTRL_UP;
    if (ay > ANALOG_DEADZONE)  held |= PSP_CTRL_DOWN;

    m_analogX = rsClamp((float(ax) / 128.f), -1.f, 1.f);
    m_analogY = rsClamp((float(ay) / 128.f), -1.f, 1.f);
    if (ax * ax + ay * ay < ANALOG_DEADZONE * ANALOG_DEADZONE) {
        m_analogX = m_analogY = 0.f;
    }

    m_pressed  = held & ~m_prev;
    m_released = m_prev & ~held;
    m_held     = held;
    m_prev     = held;

    m_repeat = 0;
    for (int i = 0; i < 4; i++) {
        if (m_held & DIRS[i]) {
            m_repeatTimer[i] += dt;
            if (m_repeatTimer[i] >= REPEAT_DELAY) {
                m_repeatTimer[i] -= REPEAT_RATE;
                m_repeat |= DIRS[i];
            }
        } else {
            m_repeatTimer[i] = 0.f;
        }
    }
}

}  // namespace rs::input
