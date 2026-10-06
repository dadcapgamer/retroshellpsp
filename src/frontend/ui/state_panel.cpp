#include "frontend/ui/state_panel.h"

#include "frontend/app.h"
#include "frontend/ui/chrome.h"
#include "frontend/ui/text_layout.h"

#include <cmath>

namespace rs::ui {

namespace {
/* The RetroShell mark at icon size, `lit` cells drawn in `hot`. */
constexpr int MARK_DOT = 5;
void markIcon(gfx::Renderer& r, float cx, float cy, u32 cold, u32 hot,
              unsigned lit, unsigned skip = 0u) {
    const float half = brandMarkSize(MARK_DOT) * .5f;
    const float x0 = float(int(cx - half)), y0 = float(int(cy - half));
    brandMark(r, x0, y0, MARK_DOT, cold, lit | skip);
    if (lit) brandMark(r, x0, y0, MARK_DOT, hot, ~lit | skip);
}
}  // namespace

void drawStatePanel(App& app, const StatePanel& panel, u32 alpha, float dy) {
    if (alpha <= 2u) return;
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    constexpr float CX = RS_SCREEN_W * .5f;
    constexpr float WIDTH = 340.f, STEP = 17.f, ICON = 25.f;

    const int lines = wrappedLineCount(fonts.body, WIDTH, 4, panel.message);
    const bool hasAction = panel.actionLabel && *panel.actionLabel;
    /* Icon, gap 14, title, gap 8, message, detail, gap 14, action. */
    const float blockH = ICON + 14.f + fonts.title.capHeight() +
                         (lines ? 9.f + float(lines) * STEP - 4.f : 0.f) +
                         (panel.detail ? 12.f + 7.f : 0.f) +
                         (hasAction ? 18.f + 10.f : 0.f);
    const float top = layout::HEADER_RULE_Y, bottom = layout::FOOTER_RULE_Y;
    float y = float(int(top + (bottom - top - blockH) * .5f)) + dy;

    const float icy = y + ICON * .5f;
    switch (panel.kind) {
        case StatePanel::Kind::Error: {
            const u32 ink = fade(pal.danger, alpha);
            pixelFrame(r, CX - 12.f, y, 25.f, 25.f, 2, 3, ink);
            r.rect(CX - 1.f, y + 6.f, 3.f, 8.f, ink);
            r.rect(CX - 1.f, y + 16.f, 3.f, 3.f, ink);
            break;
        }
        case StatePanel::Kind::Empty:
            /* The mark with its centre missing: the shell, nothing in it. */
            markIcon(r, CX, icy, fade(pal.textMuted, alpha), 0u, 0u,
                     1u << BRAND_CENTER);
            break;
        case StatePanel::Kind::Done:
            markIcon(r, CX, icy, 0u, fade(pal.focusEdge, alpha), 0x1FFu);
            break;
        case StatePanel::Kind::Loading: {
            /* One square walks the mark's outer ring — branded, and alive. */
            static const int RING[8] = {0, 2, 5, 7, 8, 6, 3, 1};
            const int step = int(app.time() * 10.f) % 8;
            markIcon(r, CX, icy, fade(pal.surface2, alpha),
                     fade(pal.focusEdge, alpha),
                     (1u << RING[step]) | (1u << RING[(step + 7) % 8]));
            break;
        }
    }
    y += ICON + 14.f;

    fonts.title.draw(r, CX, fonts.title.centerY(y, fonts.title.capHeight()),
                     panel.title, fade(pal.textPrimary, alpha),
                     text::Align::Center);
    y += fonts.title.capHeight();

    if (lines) {
        y += 9.f;
        drawWrapped(fonts.body, r, CX, fonts.body.centerY(y, fonts.body.capHeight()),
                    WIDTH, STEP, 4, panel.message,
                    fade(pal.textSecondary, alpha), text::Align::Center);
        y += float(lines) * STEP - 4.f;
    }
    if (panel.detail) {
        y += 12.f;
        fonts.small.draw(r, CX, fonts.small.centerY(y, 7.f), panel.detail,
                         fade(pal.textMuted, alpha), text::Align::Center);
        y += 7.f;
    }
    if (hasAction) {
        y += 18.f;
        const float lead = prim::buttonGlyphWidth(panel.actionButton);
        const float labelW = fonts.bodyStrong.measure(panel.actionLabel);
        const float total = lead + 7.f + labelW;
        const float x0 = float(int(CX - total * .5f));
        prim::buttonGlyph(r, panel.actionButton, x0 + lead * .5f, y + 5.f, 6.f,
                          fade(pal.textPrimary, alpha));
        fonts.bodyStrong.draw(r, x0 + lead + 7.f,
                              fonts.bodyStrong.centerY(y, 10.f),
                              panel.actionLabel, fade(pal.textPrimary, alpha));
    }
}

}  // namespace rs::ui
