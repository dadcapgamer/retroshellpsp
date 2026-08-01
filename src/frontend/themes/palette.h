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
    u32 panelBg, panelOutline;    /* content cards (translucent frosted tint) */
    u32 menuBg;                   /* opaque surface for modal dialogs */
    u32 shadow;                   /* text shadow */
    u32 scrim;                    /* scene-transition overlay base (alpha set live) */
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
        /* IPS-friendly warm grey instead of a near-white backlight field. */
        /* bg        */ rsHex(0xD5D4CF), rsHex(0xC9C8C3),
        /* waves     */ rsHex(0xD6A646, 8), rsHex(0x8F8777, 5),
        /* text      */ rsHex(0x24231F), rsHex(0x45433E), rsHex(0x605D56),
        /* accent    */ rsHex(0xD6A646),
        /* tiles     */ rsHex(0xE7E6E1, 218), rsHex(0xF7F6F2),
        /* panel     */ rsHex(0xE4E2DD, 236), rsHex(0x383630, 46),
        /* menu      */ rsHex(0xE8E6E1),
        /* shadow    */ rsHex(0x383630, 42),
        /* scrim     */ rsHex(0xC9C8C3),
        false,
    };
    return p;
}

inline const Palette& dark() {
    static const Palette p = {
        /* Solid near-black field; no gradient or decorative bands. */
        /* bg        */ rsHex(0x121318), rsHex(0x121318),
        /* waves     */ rsHex(0xD6A646, 12), rsHex(0x8E846D, 7),
        /* text      */ rsHex(0xF3EEE2), rsHex(0xC2BCB0), rsHex(0x7E7B75),
        /* accent    */ rsHex(0xE0B557),
        /* tiles     */ rsHex(0xFFFFFF, 15), rsHex(0x34363F),
        /* panel     */ rsHex(0xFFFFFF, 10), rsHex(0xFFFFFF, 24),
        /* menu      */ rsHex(0x24252B),
        /* shadow    */ rsHex(0x000000, 118),
        /* scrim     */ rsHex(0x121318),
        true,
    };
    return p;
}

/* Per-frame blend used while the theme crossfades. */
Palette blend(const Palette& a, const Palette& b, float t);

}  // namespace rs::theme
