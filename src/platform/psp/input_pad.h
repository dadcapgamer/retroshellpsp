/** PSP controller sampling with edge detection and UI key-repeat.
 *
 * `held/pressed/released` are PSP_CTRL_* bitmasks. The analog stick is
 * folded into the d-pad bits for menu navigation (games read the raw
 * analog values separately in Phase 3+). `navPressed()` adds auto-repeat
 * for held directions, which is what menu code should use.
 */
#pragma once

#include "rs_common.h"

#include <pspctrl.h>

namespace rs::input {

class Pad {
public:
    void init();
    void poll();
    /* Refresh held/analog state without consuming pressed/released edges.
     * Used between multiple emulated frames in one presentation pass. */
    void refreshHeld();

    u32 held() const     { return m_held; }
    u32 pressed() const  { return m_pressed; }
    u32 released() const { return m_released; }

    bool isPressed(u32 mask) const  { return (m_pressed & mask) != 0; }
    bool isHeld(u32 mask) const     { return (m_held & mask) != 0; }
    /* Pressed OR auto-repeated — for list/grid navigation. */
    bool navPressed(u32 mask) const { return ((m_pressed | m_repeat) & mask) != 0; }

    /* -1..1, dead-zone applied. */
    float analogX() const { return m_analogX; }
    float analogY() const { return m_analogY; }

    /* Merge synthetic button state into the next poll (autopilot builds and
     * future input-remap tests). Cleared every poll. */
    void simulate(u32 heldMask) { m_simHeld = heldMask; }

private:
    static constexpr float REPEAT_DELAY = 0.36f;
    static constexpr float REPEAT_RATE  = 0.075f;

    u32 m_held = 0, m_prev = 0, m_pressed = 0, m_released = 0, m_repeat = 0;
    u32 m_simHeld = 0;
    float m_analogX = 0.f, m_analogY = 0.f;
    float m_repeatTimer[4] = {};  /* up, down, left, right */
    u32 m_lastPollUs = 0;
};

}  // namespace rs::input
