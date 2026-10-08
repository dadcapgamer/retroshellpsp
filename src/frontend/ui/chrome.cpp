#include "frontend/ui/chrome.h"

#include "frontend/app.h"
#include "frontend/database/systems.h"
#include "frontend/ui/icons.h"

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
    app.fonts().tiny.draw(app.renderer(), x, y, text, color, align, 1.f);
}

float chipWidth(App& app, const char* text) {
    return px(app.fonts().tiny.measure(text)) + 12.f;
}

float chip(App& app, float x, float y, const char* text, u32 alpha,
           bool system) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& font = app.fonts().tiny;
    const float w = chipWidth(app, text);
    if (system) {
        pixelRect(r, x, y, w, CHIP_H, 2, fade(pal.focusEdge, alpha));
    } else {
        pixelRect(r, x, y, w, CHIP_H, 2, fade(pal.surface2, alpha));
        pixelFrame(r, x, y, w, CHIP_H, 1, 2, fade(pal.line, alpha));
    }
    font.draw(r, px(x) + 6.f, font.centerY(px(y), CHIP_H), text,
              fade(system ? pal.bg : pal.textSecondary, alpha));
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
        const int c = i % 3, row = i / 3;     /* grid cell, turned 45 deg */
        r.rect(x + float(c - row + 2) * d, y + float(c + row) * d, d, d, color);
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

void card(App& app, float x, float y, float w, float h, u32 alpha) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    pixelRect(r, x, y, w, h, 3, fade(pal.surface, alpha));
    pixelFrame(r, x, y, w, h, 1, 3, fade(pal.line, alpha));
}

void rowRule(App& app, float x, float y, float w, u32 alpha) {
    app.renderer().rect(px(x) + 6.f, px(y), px(w) - 12.f, 1.f,
                        fade(app.pal().line, alpha));
}

void menuRow(App& app, float x, float y, float w, float h, const char* text,
             const char* value, const RowStyle& style, u32 alpha) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const bool focused = style.focused;
    const bool hard = focused && !style.soft;     /* accent focus */
    if (hard) focusFill(app, x, y, w, h, alpha);
    else if (focused) pixelRect(r, x, y, w, h, 2, fade(pal.surface2, alpha));
    const text::Font& face =
        (hard || style.strong) ? fonts.bodyStrong : fonts.body;
    u32 ink = hard ? pal.onAccent : pal.textPrimary;
    if (style.disabled) ink = hard ? rsWithAlpha(pal.onAccent, 170)
                                   : pal.textDisabled;
    const float cy = px(y) + float(int(h) / 2);
    float tx = px(x) + 10.f;
    if (style.icon >= 0) {
        icon(r, Icon(style.icon), tx, cy - 5.f,
             fade(hard ? pal.onAccent
                       : style.disabled ? pal.textDisabled : pal.textSecondary,
                  alpha));
        tx += ICON_SIZE + 9.f;
    }
    face.draw(r, tx, face.centerY(y, h), text, fade(ink, alpha));

    float right = px(x + w) - 10.f;
    const u32 vink = hard ? pal.onAccent
                   : style.disabled ? pal.textDisabled
                   : focused ? pal.textPrimary : pal.textSecondary;
    if (style.chevron && hard) {
        prim::chevron(r, prim::Dir::Right, right - 2.f, cy, 4.f, 1.5f,
                      fade(pal.onAccent, alpha));
        right -= 14.f;
    }
    if (!value || !*value) return;

    const auto& vf = fonts.body;
    const bool arrows = style.adjustable && focused;
    if (arrows) {
        /* ‹ value ›: the value centred between fixed arrows, so it does not
         * jump as its width changes. */
        constexpr float SPAN = 104.f;
        const float left = right - SPAN;
        prim::chevron(r, prim::Dir::Left, left + 2.f, cy, 3.f, 1.5f,
                      fade(vink, alpha));
        prim::chevron(r, prim::Dir::Right, right - 2.f, cy, 3.f, 1.5f,
                      fade(vink, alpha));
        vf.draw(r, left + SPAN * .5f, vf.centerY(y, h), value,
                fade(vink, alpha), text::Align::Center);
        return;
    }
    vf.draw(r, style.adjustable ? right - 52.f : right, vf.centerY(y, h), value,
            fade(vink, alpha),
            style.adjustable ? text::Align::Center : text::Align::Right);
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

    /* Brand line, centred along the top: mark + RETROSHELL on the larger
     * previews, the mark alone on small tiles. */
    const bool large = w >= 140.f && h >= 100.f;
    const float brandY = y + (large ? 10.f : 8.f);
    if (w >= 96.f) {
        const char* word = "RetroShell";
        const float ww = large ? fonts.tiny.measure(word, 1.f) + 9.f : 0.f;
        const float bx = px(x + (w - (5.f + ww)) * .5f);
        brandMark(r, bx, brandY + 1.f, 1, fade(pal.textSecondary, alpha));
        if (large)
            fonts.tiny.draw(r, bx + 9.f, fonts.tiny.centerY(brandY, 7.f), word,
                            fade(pal.textSecondary, alpha), text::Align::Left,
                            1.f);
    }

    const float icon = h >= 112.f && w >= 96.f ? 64.f : 48.f;
    const char* badge = db::systemInfo(db::System(systemId)).badge;
    const bool withBadge = h >= icon + 34.f;
    const float block = icon + (withBadge ? 18.f : 0.f);
    const float iy = px(y + (h - block) * .5f + (w >= 96.f ? 4.f : 0.f));
    prim::iconSystem(r, systemId, x + px((w - icon) * .5f), iy, icon,
                     rsWithAlpha(rsHex(0xFFFFFF), alpha), pal.accent);
    if (withBadge)
        fonts.bodyStrong.draw(r, x + w * .5f,
                              fonts.bodyStrong.centerY(iy + icon + 6.f, 8.f),
                              badge, fade(pal.textPrimary, alpha),
                              text::Align::Center);
}

}  // namespace rs::ui
