#include "frontend/themes/palette.h"

namespace rs::theme {

const AccentOption& accentOption(int index) {
    static const AccentOption OPTIONS[ACCENT_COUNT] = {
        {"Cobalt", 0x2F63C8},
        {"Teal",   0x2A8C8C},
        {"Rust",   0xB5532D},
        {"Gold",   0xC9962B},
        {"Violet", 0x7B63C9},
        {"Rose",   0xBE5C82},
        {"Sage",   0x5E8A5E},
        {"Slate",  0x5B6B82},
    };
    return OPTIONS[rsClamp(index, 0, ACCENT_COUNT - 1)];
}

Palette personalize(const Palette& base, int accentIndex) {
    Palette p = base;
    const u32 rgb = accentOption(accentIndex).rgb;
    /* Dark fields need a lifted accent to keep the selection bar readable;
     * the same hue family is used so the two themes stay one identity. */
    const u32 lift = base.dark ? 0x10u : 0u;
    auto channel = [&](u32 shift) {
        const u32 c = ((rgb >> shift) & 0xFFu) + lift;
        return c > 255u ? 255u : c;
    };
    const u32 tuned = (channel(16) << 16) | (channel(8) << 8) | channel(0);
    p.accent = rsHex(tuned);
    p.selectBg = rsHex(tuned);
    p.waveA = rsHex(tuned, 0);
    p.waveB = rsHex(tuned, 0);
    return p;
}

Palette blend(const Palette& a, const Palette& b, float t) {
    if (t <= 0.f) return a;
    if (t >= 1.f) return b;
    Palette p;
    p.bgTop         = rsLerpColor(a.bgTop, b.bgTop, t);
    p.bgBottom      = rsLerpColor(a.bgBottom, b.bgBottom, t);
    p.waveA         = rsLerpColor(a.waveA, b.waveA, t);
    p.waveB         = rsLerpColor(a.waveB, b.waveB, t);
    p.textPrimary   = rsLerpColor(a.textPrimary, b.textPrimary, t);
    p.textSecondary = rsLerpColor(a.textSecondary, b.textSecondary, t);
    p.textDim       = rsLerpColor(a.textDim, b.textDim, t);
    p.accent        = rsLerpColor(a.accent, b.accent, t);
    p.tileBg        = rsLerpColor(a.tileBg, b.tileBg, t);
    p.tileFocusBg   = rsLerpColor(a.tileFocusBg, b.tileFocusBg, t);
    p.panelBg       = rsLerpColor(a.panelBg, b.panelBg, t);
    p.panelOutline  = rsLerpColor(a.panelOutline, b.panelOutline, t);
    p.menuBg        = rsLerpColor(a.menuBg, b.menuBg, t);
    p.shadow        = rsLerpColor(a.shadow, b.shadow, t);
    p.scrim         = rsLerpColor(a.scrim, b.scrim, t);
    p.selectBg      = rsLerpColor(a.selectBg, b.selectBg, t);
    p.selectText    = rsLerpColor(a.selectText, b.selectText, t);
    p.divider       = rsLerpColor(a.divider, b.divider, t);
    p.railOutline   = rsLerpColor(a.railOutline, b.railOutline, t);
    p.dim           = rsLerpColor(a.dim, b.dim, t);
    p.fallbackDot   = rsLerpColor(a.fallbackDot, b.fallbackDot, t);
    p.dark          = t < 0.5f ? a.dark : b.dark;
    return p;
}

}  // namespace rs::theme
