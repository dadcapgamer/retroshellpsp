/** Shared basic types and small helpers used across RetroShell. */
#pragma once

#include <cstdint>
#include <cstddef>

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

/* PSP GE color layout is ABGR: 0xAABBGGRR. */
constexpr u32 rsRGBA(u32 r, u32 g, u32 b, u32 a = 0xFF) {
    return (a << 24) | (b << 16) | (g << 8) | r;
}

/* 0xRRGGBB hex + alpha convenience (matches how designers read colors). */
constexpr u32 rsHex(u32 rgb, u32 a = 0xFF) {
    return rsRGBA((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, a);
}

constexpr u32 rsWithAlpha(u32 abgr, u32 a) {
    return (abgr & 0x00FFFFFFu) | (a << 24);
}

inline u32 rsAlphaOf(u32 abgr) { return abgr >> 24; }

/* Linear interpolation between two ABGR colors, t in [0,1]. */
inline u32 rsLerpColor(u32 a, u32 b, float t) {
    if (t <= 0.f) return a;
    if (t >= 1.f) return b;
    u32 out = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        int ca = (a >> shift) & 0xFF;
        int cb = (b >> shift) & 0xFF;
        out |= u32(ca + int(t * float(cb - ca))) << shift;
    }
    return out;
}

template <typename T>
constexpr T rsClamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

constexpr float rsLerp(float a, float b, float t) { return a + (b - a) * t; }

constexpr u32 rsNextPow2(u32 v) {
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

constexpr int RS_SCREEN_W = 480;
constexpr int RS_SCREEN_H = 272;
