/** Color palettes for the built-in Light and Dark themes.
 *
 * Phase 2's theme engine loads palettes (and assets) from theme.json files;
 * these two are compiled in so the app looks right with nothing installed.
 */
#pragma once

#include "rs_common.h"

namespace rs::theme {

struct Palette {
    u32 bgTop, bgBottom;          /* background gradient */
    u32 waveA, waveB;             /* animated ribbon tints (premultiplied-ish alphas) */
    u32 textPrimary, textSecondary, textDim;
    u32 accent;
    u32 tileBg, tileFocusBg;      /* category tiles */
    u32 panelBg, panelOutline;    /* content cards */
    u32 shadow;                   /* text shadow */
    u32 scrim;                    /* scene-transition overlay base (alpha set live) */
    bool dark;
};

inline const Palette& light() {
    static const Palette p = {
        /* bg        */ rsHex(0xF3F5F9), rsHex(0xD9E0EC),
        /* waves     */ rsHex(0x2E7CF6, 30), rsHex(0x74A9FF, 22),
        /* text      */ rsHex(0x1A1E26), rsHex(0x49536A), rsHex(0x8792A8),
        /* accent    */ rsHex(0x2E7CF6),
        /* tiles     */ rsHex(0xFFFFFF, 216), rsHex(0xFFFFFF),
        /* panel     */ rsHex(0xFFFFFF, 190), rsHex(0x1A1E26, 26),
        /* shadow    */ rsHex(0x39496B, 46),
        /* scrim     */ rsHex(0xEDF1F7),
        false,
    };
    return p;
}

inline const Palette& dark() {
    static const Palette p = {
        /* bg        */ rsHex(0x0E1218), rsHex(0x1A2230),
        /* waves     */ rsHex(0x3E8CFF, 34), rsHex(0x6FA8FF, 20),
        /* text      */ rsHex(0xF2F5FA), rsHex(0xA9B3C4), rsHex(0x5F6A7D),
        /* accent    */ rsHex(0x4C9AFF),
        /* tiles     */ rsHex(0xFFFFFF, 22), rsHex(0xFFFFFF, 46),
        /* panel     */ rsHex(0xFFFFFF, 14), rsHex(0xFFFFFF, 26),
        /* shadow    */ rsHex(0x000000, 110),
        /* scrim     */ rsHex(0x0B0E13),
        true,
    };
    return p;
}

/* Per-frame blend used while the theme crossfades. */
Palette blend(const Palette& a, const Palette& b, float t);

}  // namespace rs::theme
