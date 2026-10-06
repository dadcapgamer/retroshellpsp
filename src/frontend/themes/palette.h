/** Color palettes for the built-in Light and Dark themes.
 *
 * Phase 2's theme engine loads palettes (and assets) from theme.json files;
 * these two are compiled in so the app looks right with nothing installed.
 */
#pragma once

#include "rs_common.h"

namespace rs::theme {

struct Palette {
    u32 bgTop, bgBottom;          /* built-ins use one solid background */
    u32 waveA, waveB;             /* animated ribbon tints (premultiplied-ish alphas) */
    u32 textPrimary, textSecondary, textDim;
    u32 accent;
    u32 tileBg, tileFocusBg;      /* category tiles */
    u32 panelBg, panelOutline;    /* content cards (translucent frosted tint) */
    u32 menuBg;                   /* opaque surface for modal dialogs */
    u32 shadow;                   /* text shadow */
    u32 scrim;                    /* scene-transition overlay base (alpha set live) */
    u32 selectBg, selectText;     /* flat selection bar and its text */
    u32 divider;                  /* thin separators and the legend rule */
    u32 railOutline;              /* box around the selected system */
    u32 dim;                      /* backdrop under popups (alpha is live) */
    u32 fallbackDot;              /* dotted placeholder pattern */
    bool dark;
};

struct AccentOption {
    const char* name;
    u32 rgb;
};

constexpr int ACCENT_COUNT = 8;
const AccentOption& accentOption(int index);
Palette personalize(const Palette& base, int accentIndex);

inline const Palette& light() {
    static const Palette p = {
        /* Warm ivory field: flat, no gradient, no ribbons. */
        /* bg        */ rsHex(0xF1EBDD), rsHex(0xF1EBDD),
        /* waves     */ rsHex(0x2F63C8, 0), rsHex(0x2F63C8, 0),
        /* text      */ rsHex(0x1B2230), rsHex(0x4A5160), rsHex(0x7C8089),
        /* accent    */ rsHex(0x2F63C8),
        /* tiles     */ rsHex(0xE6DFCE), rsHex(0xE6DFCE),
        /* panel     */ rsHex(0xE9E2D2), rsHex(0x1B2230, 70),
        /* menu      */ rsHex(0xF6F1E4),
        /* shadow    */ rsHex(0x1B2230, 40),
        /* scrim     */ rsHex(0xF1EBDD),
        /* select    */ rsHex(0x2F63C8), rsHex(0xFFFFFF),
        /* divider   */ rsHex(0x1B2230, 56),
        /* rail box  */ rsHex(0x1B2230),
        /* dim       */ rsHex(0x0B1220),
        /* dots      */ rsHex(0x1B2230, 34),
        false,
    };
    return p;
}

inline const Palette& dark() {
    static const Palette p = {
        /* Deep navy-charcoal field, same flat treatment. */
        /* bg        */ rsHex(0x0F1829), rsHex(0x0F1829),
        /* waves     */ rsHex(0x3B73E0, 0), rsHex(0x3B73E0, 0),
        /* text      */ rsHex(0xEFE9DA), rsHex(0xB9B6AC), rsHex(0x7D8594),
        /* accent    */ rsHex(0x3B73E0),
        /* tiles     */ rsHex(0x172338), rsHex(0x172338),
        /* panel     */ rsHex(0x15213A), rsHex(0xEFE9DA, 60),
        /* menu      */ rsHex(0x16233B),
        /* shadow    */ rsHex(0x000000, 118),
        /* scrim     */ rsHex(0x0F1829),
        /* select    */ rsHex(0x3B73E0), rsHex(0xFFFFFF),
        /* divider   */ rsHex(0xEFE9DA, 50),
        /* rail box  */ rsHex(0xEFE9DA),
        /* dim       */ rsHex(0x000000),
        /* dots      */ rsHex(0xEFE9DA, 26),
        true,
    };
    return p;
}

/* Per-frame blend used while the theme crossfades. */
Palette blend(const Palette& a, const Palette& b, float t);

}  // namespace rs::theme
