#include "frontend/themes/palette.h"

namespace rs::theme {

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
    p.shadow        = rsLerpColor(a.shadow, b.shadow, t);
    p.scrim         = rsLerpColor(a.scrim, b.scrim, t);
    p.dark          = t < 0.5f ? a.dark : b.dark;
    return p;
}

}  // namespace rs::theme
