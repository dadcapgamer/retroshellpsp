/** Design tokens for the built-in themes: Dark (navy, the default),
 * Graphite, Light and Mist.
 *
 * Every colour in the shell comes from one of these roles, so a screen never
 * invents its own. Accent means exactly one thing everywhere: focus, the
 * active state. Surfaces are flat; depth comes from the three-step
 * bg -> surface -> surface2 ladder, never from blur or shadows.
 */
#pragma once

#include "rs_common.h"

namespace rs::theme {

struct Palette {
    u32 bg;                       /* main field */
    u32 surface;                  /* raised surface: cards, panes */
    u32 surface2;                 /* secondary surface: popups, chips, art wells */
    u32 line;                     /* hairlines: header/footer rules, borders */
    u32 textPrimary, textSecondary, textMuted, textDisabled;
    u32 accent;                   /* focus fill */
    u32 onAccent;                 /* text and glyphs on the focus fill */
    u32 focusEdge;                /* focus underline / frame (brighter accent) */
    u32 danger;                   /* low battery, missing emulator */
    u32 scrim;                    /* scene-transition overlay base (alpha set live) */
    u32 dim;                      /* backdrop under popups (alpha is live) */
    u32 pattern;                  /* dotted fallback-art pattern */
    u32 waveA, waveB;             /* legacy custom-theme ribbons only */
    bool dark;
};

struct AccentOption {
    const char* name;
    u32 rgb;
};

constexpr int ACCENT_COUNT = 5;
const AccentOption& accentOption(int index);
Palette personalize(const Palette& base, int accentIndex);

inline const Palette& dark() {
    static const Palette p = {
        /* bg        */ rsHex(0x081828),
        /* surface   */ rsHex(0x0D2235),
        /* surface2  */ rsHex(0x122B40),
        /* line      */ rsHex(0x24435F),
        /* text      */ rsHex(0xF4F1E8), rsHex(0xADB8C2), rsHex(0x718292),
                        rsHex(0x4D5D69),
        /* accent    */ rsHex(0x2676E8),
        /* onAccent  */ rsHex(0xFFFFFF),
        /* focusEdge */ rsHex(0x54B7FF),
        /* danger    */ rsHex(0xF0645A),
        /* scrim     */ rsHex(0x081828),
        /* dim       */ rsHex(0x030A12),
        /* pattern   */ rsHex(0x1C3A54),
        /* waves     */ rsHex(0x2676E8, 0), rsHex(0x2676E8, 0),
        true,
    };
    return p;
}

inline const Palette& light() {
    static const Palette p = {
        /* Same roles and contrast steps as Dark, on warm paper. */
        /* bg        */ rsHex(0xF2EFE7),
        /* surface   */ rsHex(0xE8E4DA),
        /* surface2  */ rsHex(0xDDD8CC),
        /* line      */ rsHex(0xCCC6B8),
        /* text      */ rsHex(0x14202C), rsHex(0x44515E), rsHex(0x6D7884),
                        rsHex(0xA3ABB3),
        /* accent    */ rsHex(0x2676E8),
        /* onAccent  */ rsHex(0xFFFFFF),
        /* focusEdge */ rsHex(0x1C5FC4),
        /* danger    */ rsHex(0xC8402F),
        /* scrim     */ rsHex(0xF2EFE7),
        /* dim       */ rsHex(0x14202C),
        /* pattern   */ rsHex(0xCFC9BB),
        /* waves     */ rsHex(0x2676E8, 0), rsHex(0x2676E8, 0),
        false,
    };
    return p;
}

/* Neutral charcoal: the Dark structure without the blue cast. */
inline const Palette& graphite() {
    static const Palette p = {
        /* bg        */ rsHex(0x131517),
        /* surface   */ rsHex(0x1B1E21),
        /* surface2  */ rsHex(0x23272B),
        /* line      */ rsHex(0x343A40),
        /* text      */ rsHex(0xF2F1EE), rsHex(0xB3B7BB), rsHex(0x7C8288),
                        rsHex(0x4F555B),
        /* accent    */ rsHex(0x2676E8),
        /* onAccent  */ rsHex(0xFFFFFF),
        /* focusEdge */ rsHex(0x54B7FF),
        /* danger    */ rsHex(0xF0645A),
        /* scrim     */ rsHex(0x131517),
        /* dim       */ rsHex(0x000000),
        /* pattern   */ rsHex(0x2B3035),
        /* waves     */ rsHex(0x2676E8, 0), rsHex(0x2676E8, 0),
        true,
    };
    return p;
}

/* Cool grey-blue daylight: Light's structure without the warmth. */
inline const Palette& mist() {
    static const Palette p = {
        /* bg        */ rsHex(0xEDF1F5),
        /* surface   */ rsHex(0xE2E8EE),
        /* surface2  */ rsHex(0xD6DEE6),
        /* line      */ rsHex(0xC1CBD5),
        /* text      */ rsHex(0x121C26), rsHex(0x3E4B58), rsHex(0x65727E),
                        rsHex(0x9FA9B3),
        /* accent    */ rsHex(0x2676E8),
        /* onAccent  */ rsHex(0xFFFFFF),
        /* focusEdge */ rsHex(0x1C5FC4),
        /* danger    */ rsHex(0xC8402F),
        /* scrim     */ rsHex(0xEDF1F5),
        /* dim       */ rsHex(0x121C26),
        /* pattern   */ rsHex(0xC8D1DA),
        /* waves     */ rsHex(0x2676E8, 0), rsHex(0x2676E8, 0),
        false,
    };
    return p;
}

/* Per-frame blend used while the theme crossfades. */
Palette blend(const Palette& a, const Palette& b, float t);

}  // namespace rs::theme
