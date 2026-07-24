/** Animation primitives — everything in the UI that moves goes through
 * these, which is what keeps navigation feeling fluid at 60 fps. */
#pragma once

#include "rs_common.h"

#include <cmath>

namespace rs::ui {

/* Framerate-independent exponential approach: great for focus/scroll
 * movement because retargeting mid-flight stays smooth. */
struct Smooth {
    float v = 0.f, target = 0.f;

    void snap(float t)  { v = target = t; }
    void to(float t)    { target = t; }
    bool settled(float eps = 0.01f) const { return std::fabs(v - target) < eps; }

    float update(float dt, float speed = 14.f) {
        v += (target - v) * (1.f - std::exp(-speed * dt));
        if (settled(0.001f)) v = target;
        return v;
    }
};

/* One-shot timed animation, 0 → 1. */
struct Tween {
    float t = 1.f, duration = 0.25f;

    void start(float dur) { duration = dur; t = 0.f; }
    bool running() const  { return t < 1.f; }

    float update(float dt) {
        if (t < 1.f) t = rsClamp(t + dt / duration, 0.f, 1.f);
        return t;
    }
};

inline float easeOutCubic(float t) {
    const float u = 1.f - t;
    return 1.f - u * u * u;
}

inline float easeOutQuint(float t) {
    const float u = 1.f - t;
    return 1.f - u * u * u * u * u;
}

inline float easeInOutQuad(float t) {
    return t < 0.5f ? 2.f * t * t : 1.f - 2.f * (1.f - t) * (1.f - t);
}

/* Slight overshoot — used for focus pops. */
inline float easeOutBack(float t) {
    const float c1 = 1.70158f, c3 = c1 + 1.f;
    const float u = t - 1.f;
    return 1.f + c3 * u * u * u + c1 * u * u;
}

}  // namespace rs::ui
