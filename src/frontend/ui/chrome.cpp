#include "frontend/ui/chrome.h"

#include "frontend/app.h"
#include "frontend/database/systems.h"

namespace rs::ui {

namespace {
float px(float v) { return float(int(v)); }
}  // namespace

const char* systemName(int systemId) {
    static const char* NAMES[] = {
        "Game Boy", "Game Boy Color", "Game Boy Advance", "NES",
        "Super Nintendo", "Genesis", "Master System", "Game Gear",
        "PC Engine",
    };
    return NAMES[rsClamp(systemId, 0, db::SYSTEM_COUNT - 1)];
}

/* ---------------------------------------------------------------------- */
/* Surfaces                                                                */
/* ---------------------------------------------------------------------- */

void pixelRect(gfx::Renderer& r, float x, float y, float w, float h,
               int radius, u32 color) {
    x = px(x); y = px(y); w = px(w); h = px(h);
    if (w <= 0.f || h <= 0.f) return;
    radius = rsClamp(radius, 0, 3);
    if (radius * 2 > int(h)) radius = int(h) / 2;
    if (radius * 2 > int(w)) radius = int(w) / 2;
    if (radius == 0) {
        r.rect(x, y, w, h, color);
        return;
    }
    /* Corner steps per row from the edge: r=1 {1}, r=2 {1}, r=3 {2,1}. */
    const int steps3[] = {2, 1};
    const int rows = radius == 3 ? 2 : 1;
    for (int i = 0; i < rows; i++) {
        const float inset = float(radius == 3 ? steps3[i] : 1);
        r.rect(x + inset, y + float(i), w - 2.f * inset, 1.f, color);
        r.rect(x + inset, y + h - 1.f - float(i), w - 2.f * inset, 1.f, color);
    }
    r.rect(x, y + float(rows), w, h - 2.f * float(rows), color);
}

void pixelFrame(gfx::Renderer& r, float x, float y, float w, float h, int t,
                int radius, u32 color) {
    x = px(x); y = px(y); w = px(w); h = px(h);
    t = rsClamp(t, 1, 2);
    radius = rsClamp(radius, 0, 3);
    if (w < 2.f * float(t) + 2.f || h < 2.f * float(t) + 2.f) return;
    const float c = float(radius > 0 ? (radius == 3 ? 2 : 1) : 0);
    const float tf = float(t);
    /* Top and bottom bands, inset at the corners. */
    for (int i = 0; i < t; i++) {
        const float inset = i == 0 ? c : (c > 1.f ? 1.f : 0.f);
        r.rect(x + inset, y + float(i), w - 2.f * inset, 1.f, color);
        r.rect(x + inset, y + h - 1.f - float(i), w - 2.f * inset, 1.f, color);
    }
    /* Sides between the bands; a radius-3 frame needs one extra corner row. */
    float top = y + tf, bottom = y + h - tf;
    if (radius == 3 && t == 1) {
        r.rect(x + 1.f, y + 1.f, 1.f, 1.f, color);
        r.rect(x + w - 2.f, y + 1.f, 1.f, 1.f, color);
        r.rect(x + 1.f, y + h - 2.f, 1.f, 1.f, color);
        r.rect(x + w - 2.f, y + h - 2.f, 1.f, 1.f, color);
        top += 1.f;
        bottom -= 1.f;
    }
    r.rect(x, top, tf, bottom - top, color);
    r.rect(x + w - tf, top, tf, bottom - top, color);
}

/* ---------------------------------------------------------------------- */
/* Focus                                                                   */
/* ---------------------------------------------------------------------- */

void focusFill(App& app, float x, float y, float w, float h, u32 alpha) {
    pixelRect(app.renderer(), x, y, w, h, 2, fade(app.pal().accent, alpha));
}

void focusUnderline(App& app, float cx, float y, float w, u32 alpha) {
    app.renderer().rect(px(cx - w * .5f), px(y), px(w), 2.f,
                        fade(app.pal().focusEdge, alpha));
}

void focusFrame(App& app, float x, float y, float w, float h, u32 alpha) {
    pixelFrame(app.renderer(), x - 3.f, y - 3.f, w + 6.f, h + 6.f, 2, 3,
               fade(app.pal().focusEdge, alpha));
}

/* ---------------------------------------------------------------------- */
/* Text                                                                    */
/* ---------------------------------------------------------------------- */

void label(App& app, float x, float y, const char* text, u32 color,
           text::Align align) {
    app.fonts().monoTiny.draw(app.renderer(), x, y, text, color, align);
}

float chipWidth(App& app, const char* text) {
    return px(app.fonts().monoTiny.measure(text)) + 10.f;
}

float chip(App& app, float x, float y, const char* text, u32 alpha,
           bool onAccent) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& font = app.fonts().monoTiny;
    const float w = chipWidth(app, text);
    pixelRect(r, x, y, w, CHIP_H, 2,
              fade(onAccent ? rsWithAlpha(pal.onAccent, 48) : pal.surface2,
                   alpha));
    font.draw(r, px(x) + 5.f, font.centerY(px(y), CHIP_H), text,
              fade(onAccent ? pal.onAccent : pal.textSecondary, alpha));
    return w;
}

/* ---------------------------------------------------------------------- */
/* Brand                                                                   */
/* ---------------------------------------------------------------------- */

float brandMarkSize(int dot) { return float(dot * 5); }

void brandMark(gfx::Renderer& r, float x, float y, int dot, u32 color,
               unsigned skip) {
    x = px(x); y = px(y);
    const float d = float(dot);
    for (int i = 0; i < BRAND_CELLS; i++) {
        if ((skip >> i) & 1u) continue;
        r.rect(x + float(BRAND_CELL[i][0]) * d, y + float(BRAND_CELL[i][1]) * d,
               d, d, color);
    }
}

/* ---------------------------------------------------------------------- */
/* Panels                                                                  */
/* ---------------------------------------------------------------------- */

void backdrop(App& app, u32 alpha) {
    const auto& pal = app.pal();
    app.renderer().rect(0, 0, RS_SCREEN_W, RS_SCREEN_H,
                        rsWithAlpha(pal.dim, alpha * (pal.dark ? 170u : 110u) /
                                                 255u));
}

void panel(App& app, float x, float y, float w, float h, u32 alpha) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    pixelRect(r, x, y, w, h, 3, fade(pal.surface2, alpha));
    pixelFrame(r, x, y, w, h, 1, 3, fade(pal.line, alpha));
}

void menuRow(App& app, float x, float y, float w, float h, const char* text,
             const char* value, const RowStyle& style, u32 alpha) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const bool focused = style.focused;
    if (focused) focusFill(app, x, y, w, h, alpha);
    const text::Font& face =
        (focused || style.strong) ? fonts.bodyStrong : fonts.body;
    u32 ink = focused ? pal.onAccent : pal.textPrimary;
    if (style.disabled) ink = focused ? rsWithAlpha(pal.onAccent, 170)
                                      : pal.textDisabled;
    face.draw(r, px(x) + 10.f, face.centerY(y, h), text, fade(ink, alpha));
    if (!value || !*value) return;

    const auto& vf = fonts.mono;
    const u32 vink = focused ? pal.onAccent
                   : style.disabled ? pal.textDisabled : pal.textSecondary;
    float right = px(x + w) - 10.f;
    const float cy = px(y) + float(int(h) / 2);
    const bool arrows = style.adjustable && focused;
    if (arrows) {
        prim::chevron(r, prim::Dir::Right, right - 2.f, cy, 3.f, 1.5f,
                      fade(vink, alpha));
        right -= 12.f;
    }
    vf.draw(r, right, vf.centerY(y, h), value, fade(vink, alpha),
            text::Align::Right);
    if (arrows)
        prim::chevron(r, prim::Dir::Left, right - vf.measure(value) - 8.f, cy,
                      3.f, 1.5f, fade(vink, alpha));
}

/* ---------------------------------------------------------------------- */
/* Artwork                                                                 */
/* ---------------------------------------------------------------------- */

void artWell(App& app, const gfx::Texture& art, float x, float y, float w,
             float h, u32 alpha) {
    auto& r = app.renderer();
    x = px(x); y = px(y); w = px(w); h = px(h);
    const float sx = w / float(art.width), sy = h / float(art.height);
    const float s = sx < sy ? sx : sy;
    const float dw = px(float(art.width) * s + .5f);
    const float dh = px(float(art.height) * s + .5f);
    if (dw < w - 1.f || dh < h - 1.f)
        r.rect(x, y, w, h, fade(app.pal().surface2, alpha));
    r.sprite(art, 0.f, 0.f, float(art.width), float(art.height),
             x + px((w - dw) * .5f), y + px((h - dh) * .5f), dw, dh,
             rsWithAlpha(rsHex(0xFFFFFF), alpha));
}

void artFallback(App& app, int systemId, float x, float y, float w, float h,
                 u32 alpha) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    x = px(x); y = px(y); w = px(w); h = px(h);
    systemId = rsClamp(systemId, 0, db::SYSTEM_COUNT - 1);

    r.rect(x, y, w, h, fade(pal.surface2, alpha));
    prim::dotField(r, x, y, w, h, fade(pal.pattern, alpha));

    /* Brand line along the top edge on the single, larger previews; small
     * tiles that repeat in a row keep only the mark. */
    if (w >= 104.f && h >= 80.f) brandMark(r, x + 8.f, y + 8.f, 1,
                                          fade(pal.textMuted, alpha));
    if (w >= 140.f && h >= 100.f) {
        fonts.monoTiny.draw(r, x + 18.f, fonts.monoTiny.centerY(y + 7.f, 7.f),
                            "RETROSHELL", fade(pal.textMuted, alpha));
    }

    const float icon = h >= 112.f && w >= 96.f ? 64.f : 48.f;
    const char* badge = db::systemInfo(db::System(systemId)).badge;
    const bool withBadge = h >= icon + 30.f;
    const float block = icon + (withBadge ? 16.f : 0.f);
    const float iy = px(y + (h - block) * .5f + (w >= 104.f ? 3.f : 0.f));
    prim::iconSystem(r, systemId, x + px((w - icon) * .5f), iy, icon,
                     rsWithAlpha(rsHex(0xFFFFFF), alpha * 235u / 255u),
                     pal.accent);
    if (withBadge)
        fonts.monoTiny.draw(r, x + w * .5f, iy + icon + 6.f, badge,
                            fade(pal.textSecondary, alpha),
                            text::Align::Center);
}

}  // namespace rs::ui
