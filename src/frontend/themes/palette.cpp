#include "frontend/themes/palette.h"

namespace rs::theme {

const AccentOption& accentOption(int index) {
    static const AccentOption OPTIONS[ACCENT_COUNT] = {
        {"Blue",   0x2676E8},
        {"Teal",   0x14A08F},
        {"Violet", 0x7A5AE0},
        {"Rose",   0xD9487A},
        {"Amber",  0xD9861A},
    };
    return OPTIONS[rsClamp(index, 0, ACCENT_COUNT - 1)];
}

namespace {
/* Mix two 0xRRGGBB colours, t in 0..255 toward `to`. */
u32 mixRgb(u32 from, u32 to, u32 t) {
    auto ch = [&](u32 shift) {
        const u32 a = (from >> shift) & 0xFFu, b = (to >> shift) & 0xFFu;
        return (a * (255u - t) + b * t) / 255u;
    };
    return (ch(16) << 16) | (ch(8) << 8) | ch(0);
}
}  // namespace

Palette personalize(const Palette& base, int accentIndex) {
    Palette p = base;
    const u32 rgb = accentOption(accentIndex).rgb;
    p.accent = rsHex(rgb);
    /* The focus edge is the accent pushed toward the field's opposite: a
     * lighter tint on dark, a deeper shade on light, so a 2px underline or
     * frame reads at a glance on either. */
    p.focusEdge = base.dark ? rsHex(mixRgb(rgb, 0xFFFFFF, 92))
                            : rsHex(mixRgb(rgb, 0x000000, 46));
    p.waveA = rsHex(rgb, 0);
    p.waveB = rsHex(rgb, 0);
    return p;
}

Palette blend(const Palette& a, const Palette& b, float t) {
    if (t <= 0.f) return a;
    if (t >= 1.f) return b;
    Palette p;
    p.bg            = rsLerpColor(a.bg, b.bg, t);
    p.surface       = rsLerpColor(a.surface, b.surface, t);
    p.surface2      = rsLerpColor(a.surface2, b.surface2, t);
    p.line          = rsLerpColor(a.line, b.line, t);
    p.textPrimary   = rsLerpColor(a.textPrimary, b.textPrimary, t);
    p.textSecondary = rsLerpColor(a.textSecondary, b.textSecondary, t);
    p.textMuted     = rsLerpColor(a.textMuted, b.textMuted, t);
    p.textDisabled  = rsLerpColor(a.textDisabled, b.textDisabled, t);
    p.accent        = rsLerpColor(a.accent, b.accent, t);
    p.onAccent      = rsLerpColor(a.onAccent, b.onAccent, t);
    p.focusEdge     = rsLerpColor(a.focusEdge, b.focusEdge, t);
    p.danger        = rsLerpColor(a.danger, b.danger, t);
    p.scrim         = rsLerpColor(a.scrim, b.scrim, t);
    p.dim           = rsLerpColor(a.dim, b.dim, t);
    p.pattern       = rsLerpColor(a.pattern, b.pattern, t);
    p.waveA         = rsLerpColor(a.waveA, b.waveA, t);
    p.waveB         = rsLerpColor(a.waveB, b.waveB, t);
    p.dark          = t < 0.5f ? a.dark : b.dark;
    return p;
}

}  // namespace rs::theme
