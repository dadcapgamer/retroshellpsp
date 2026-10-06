#include "frontend/ui/state_panel.h"

#include "frontend/app.h"
#include "frontend/ui/text_layout.h"

#include <cmath>

namespace rs::ui {

namespace {
u32 fade(u32 color, u32 alpha) {
    return rsWithAlpha(color, rsAlphaOf(color) * alpha / 255u);
}
}  // namespace

void drawStatePanel(App& app, const StatePanel& panel, u32 alpha, float dy) {
    if (alpha <= 2u) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    constexpr float CX = RS_SCREEN_W * .5f;
    constexpr float WIDTH = 360.f, STEP = 16.f;

    const int lines =
        wrappedLineCount(fonts.pixelSmall, WIDTH, 4, panel.message);
    const bool hasAction = panel.actionLabel && *panel.actionLabel;
    /* Icon 36, gap 14, title 16, gap 10, message, gap 14, action 14. */
    const float blockH = 36.f + 14.f + 16.f +
                         (lines ? 10.f + float(lines) * STEP : 0.f) +
                         (panel.detail ? 8.f + 12.f : 0.f) +
                         (hasAction ? 14.f + 14.f : 0.f);
    float y = float(int((RS_SCREEN_H - blockH) * .5f)) - 6.f + dy;

    /* Icon: flat 36px glyph, no illustration. */
    const float ix = CX - 18.f;
    switch (panel.kind) {
        case StatePanel::Kind::Error:
            prim::outlineRect(r, ix, y, 36.f, 36.f, 2.f,
                              fade(pal.textPrimary, alpha));
            fonts.pixel.drawBold(r, CX, y + 8.f, "!",
                                 fade(pal.textPrimary, alpha),
                                 text::Align::Center);
            break;
        case StatePanel::Kind::Empty:
            /* An empty tray: an outlined box with a slot — an icon, never a
             * blank image placeholder. */
            prim::outlineRect(r, ix, y + 6.f, 36.f, 26.f, 2.f,
                              fade(pal.textSecondary, alpha));
            r.rect(ix + 11.f, y + 18.f, 14.f, 2.f,
                   fade(pal.textSecondary, alpha));
            break;
        case StatePanel::Kind::Done: {
            prim::outlineRect(r, ix, y, 36.f, 36.f, 2.f,
                              fade(pal.textPrimary, alpha));
            const u32 ink = fade(pal.accent, alpha);
            r.line(ix + 9.f, y + 19.f, ix + 15.f, y + 25.f, 3.f, ink);
            r.line(ix + 15.f, y + 25.f, ix + 27.f, y + 11.f, 3.f, ink);
            break;
        }
        case StatePanel::Kind::Loading: {
            prim::ring(r, CX, y + 18.f, 14.f,
                       fade(pal.textSecondary, alpha * 120u / 255u));
            const float a = app.time() * 5.f;
            prim::circle(r, CX + std::cos(a) * 14.f, y + 18.f + std::sin(a) * 14.f,
                         3.f, fade(pal.accent, alpha));
            break;
        }
    }
    y += 36.f + 14.f;

    fonts.pixelMedium.drawBold(r, CX, y, panel.title,
                               fade(pal.textPrimary, alpha),
                               text::Align::Center);
    y += 16.f;

    if (lines) {
        y += 10.f;
        drawWrapped(fonts.pixelSmall, r, CX, y, WIDTH, STEP, 4, panel.message,
                    fade(pal.textSecondary, alpha), text::Align::Center);
        y += float(lines) * STEP;
    }
    if (panel.detail) {
        y += 8.f;
        fonts.pixelTiny.draw(r, CX, y, panel.detail,
                             fade(pal.textSecondary, alpha),
                             text::Align::Center);
        y += 12.f;
    }
    if (hasAction) {
        y += 14.f;
        const float lead = prim::buttonGlyphWidth(panel.actionButton);
        const float labelW = fonts.pixelSmall.measure(panel.actionLabel);
        const float total = lead + 8.f + labelW;
        const float x0 = CX - total * .5f;
        prim::buttonGlyph(r, panel.actionButton, x0 + lead * .5f, y + 6.f, 6.f,
                          fade(pal.textPrimary, alpha));
        fonts.pixelSmall.draw(r, x0 + lead + 8.f, y, panel.actionLabel,
                              fade(pal.textPrimary, alpha));
    }
}

}  // namespace rs::ui
