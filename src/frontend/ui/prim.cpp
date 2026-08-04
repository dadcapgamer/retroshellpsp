#include "frontend/ui/prim.h"
#include "runtime/log.h"

#include "rs_asset_game_boy_png.h"
#include "rs_asset_game_boy_color_png.h"
#include "rs_asset_game_boy_advance_png.h"
#include "rs_asset_nes_png.h"
#include "rs_asset_snes_png.h"
#include "rs_asset_megadrive_png.h"
#include "rs_asset_master_system_png.h"
#include "rs_asset_game_gear_png.h"
#include "rs_asset_pc_engine_png.h"
#include "stb_image.h"

#include <pspgu.h>

#include <cmath>
#include <cstring>

namespace rs::ui::prim {

namespace {

/* Rounded-corner mask: a 32x32 tile whose corners have radius 14. Drawn
 * 9-sliced so any rect size shares one texture. */
constexpr int TILE   = 32;
constexpr int CORNER = 14;
gfx::Texture s_round;      /* filled */
gfx::Texture s_roundRing;  /* 1.5px outline */
gfx::Texture s_disc;       /* 32x32 filled AA circle */
gfx::Texture s_discRing;   /* 32x32 AA ring */
constexpr int SYSTEM_COUNT = 10;
constexpr int SYSTEM_CELL = 32;
constexpr int CONSOLE_COUNT = SYSTEM_COUNT - 1;
constexpr int PC_ENGINE_ICON = 8;
gfx::Texture s_systemIcons[CONSOLE_COUNT];

float roundedCoverage(float px, float py, float w, float h, float rad) {
    /* Signed distance to a rounded rectangle centered in [0,w]x[0,h]. */
    const float cx = w * 0.5f, cy = h * 0.5f;
    const float qx = std::fabs(px - cx) - (cx - rad);
    const float qy = std::fabs(py - cy) - (cy - rad);
    const float ax = qx > 0.f ? qx : 0.f;
    const float ay = qy > 0.f ? qy : 0.f;
    const float outside = std::sqrt(ax * ax + ay * ay);
    const float inside  = (qx > qy ? qx : qy);
    const float d = (outside > 0.f ? outside : inside) - rad;
    return rsClamp(0.5f - d, 0.f, 1.f);
}

bool bakeMasks() {
    u8 buf[TILE * TILE];

    /* Filled rounded tile. */
    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++)
            buf[y * TILE + x] = u8(255.f * roundedCoverage(
                float(x) + .5f, float(y) + .5f, TILE, TILE, CORNER));
    if (!gfx::Renderer::createTexture(s_round, TILE, TILE, GU_PSM_T8, buf,
                                      /*dynamic=*/true))
        return false;

    /* Rounded ring (outline). */
    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++) {
            const float outer = roundedCoverage(float(x) + .5f, float(y) + .5f,
                                                TILE, TILE, CORNER);
            const float inner = roundedCoverage(float(x) + .5f, float(y) + .5f,
                                                TILE, TILE, CORNER + 1.5f);
            /* inner mask shrunk 1.5px via radius trick isn't exact for
             * edges; combine with a plain inset instead. */
            float innerCov = 1.f;
            const float inset = 1.6f;
            if (x + .5f < inset || x + .5f > TILE - inset ||
                y + .5f < inset || y + .5f > TILE - inset)
                innerCov = 0.f;
            else
                innerCov = roundedCoverage(float(x) + .5f - inset,
                                           float(y) + .5f - inset,
                                           TILE - 2 * inset, TILE - 2 * inset,
                                           CORNER - inset);
            (void)inner;
            const float cov = outer - innerCov;
            buf[y * TILE + x] = u8(255.f * rsClamp(cov, 0.f, 1.f));
        }
    if (!gfx::Renderer::createTexture(s_roundRing, TILE, TILE, GU_PSM_T8, buf,
                                      /*dynamic=*/true))
        return false;

    /* Disc + ring. */
    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++) {
            const float d = std::sqrt(std::pow(float(x) + .5f - 16.f, 2.f) +
                                      std::pow(float(y) + .5f - 16.f, 2.f));
            buf[y * TILE + x] = u8(255.f * rsClamp(15.5f - d, 0.f, 1.f));
        }
    if (!gfx::Renderer::createTexture(s_disc, TILE, TILE, GU_PSM_T8, buf,
                                      /*dynamic=*/true))
        return false;

    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++) {
            const float d = std::sqrt(std::pow(float(x) + .5f - 16.f, 2.f) +
                                      std::pow(float(y) + .5f - 16.f, 2.f));
            const float cov = rsClamp(15.5f - d, 0.f, 1.f) -
                              rsClamp(13.2f - d, 0.f, 1.f);
            buf[y * TILE + x] = u8(255.f * rsClamp(cov, 0.f, 1.f));
        }
    if (!gfx::Renderer::createTexture(s_discRing, TILE, TILE, GU_PSM_T8, buf,
                                      /*dynamic=*/true))
        return false;

    s_round.clut = s_roundRing.clut = s_disc.clut = s_discRing.clut =
        gfx::Renderer::alphaClut();
    return true;
}

bool bakeSystemIcons() {
    struct EmbeddedPng {
        const unsigned char* bytes;
        unsigned int length;
    };
    /* Keep this order identical to db::System. Figma places PC Engine before
     * the Sega systems, so an explicit table prevents an atlas/order mix-up. */
    const EmbeddedPng icons[CONSOLE_COUNT] = {
        {rs_asset_game_boy_png,         rs_asset_game_boy_png_len},
        {rs_asset_game_boy_color_png,   rs_asset_game_boy_color_png_len},
        {rs_asset_game_boy_advance_png, rs_asset_game_boy_advance_png_len},
        {rs_asset_nes_png,              rs_asset_nes_png_len},
        {rs_asset_snes_png,             rs_asset_snes_png_len},
        {rs_asset_megadrive_png,        rs_asset_megadrive_png_len},
        {rs_asset_master_system_png,    rs_asset_master_system_png_len},
        {rs_asset_game_gear_png,        rs_asset_game_gear_png_len},
        {rs_asset_pc_engine_png,        rs_asset_pc_engine_png_len},
    };

    for (int icon = 0; icon < CONSOLE_COUNT; ++icon) {
        u8 pixels[SYSTEM_CELL * SYSTEM_CELL * 4] = {};
        int w = 0, h = 0, comp = 0;
        stbi_uc* source =
            stbi_load_from_memory(icons[icon].bytes, int(icons[icon].length),
                                  &w, &h, &comp, 4);
        if (!source || w != 96 || h != 96) {
            RS_LOGE("ui: console icon %d is not a valid 96x96 RGBA asset",
                    icon);
            if (source) stbi_image_free(source);
            return false;
        }

        /* Each authored Figma pixel is a 6x6 square. Sample its center and
         * write it as 2x2 so the 32px atlas remains pixel-perfect at both
         * the 32px card and 64px fallback sizes. The two near-black colors
         * are the icon-frame background in the exported nodes. */
        for (int y = 0; y < SYSTEM_CELL; ++y) {
            const int sy = (y / 2) * 6 + 3;
            for (int x = 0; x < SYSTEM_CELL; ++x) {
                const int sx = (x / 2) * 6 + 3;
                const u8* src = source + (sy * w + sx) * 4;
                u8* dst = pixels + (y * SYSTEM_CELL + x) * 4;
                const bool frameBackground =
                    src[0] <= 24 && src[1] <= 24 && src[2] <= 29;
                if (!frameBackground) {
                    /* The PC Engine's authored chassis is nearly white, so it
                     * disappears into the light-theme cards. Use a quiet cool
                     * gray for only those pale chassis pixels; preserve its
                     * red and black details and every other console asset. */
                    const bool palePcEngineChassis =
                        icon == PC_ENGINE_ICON &&
                        src[0] >= 224 && src[1] >= 224 && src[2] >= 224;
                    dst[0] = palePcEngineChassis ? 0xB8 : src[0];
                    dst[1] = palePcEngineChassis ? 0xC0 : src[1];
                    dst[2] = palePcEngineChassis ? 0xC4 : src[2];
                    dst[3] = src[3];
                }
            }
        }
        stbi_image_free(source);
        /* Keep each system in its own small texture. On PSP hardware a
         * damaged/wrapped atlas lookup contaminated every console card with
         * the same neighbouring red/green pixels. Independent 32x32 images
         * also make texture bounds exact and preserve the same VRAM cost. */
        if (!gfx::Renderer::createTexture(
                s_systemIcons[icon], SYSTEM_CELL, SYSTEM_CELL,
                GU_PSM_8888, pixels, /*dynamic=*/true))
            return false;
    }
    return true;
}

/* Draw `tex` 9-sliced with corner size `c` scaled from the baked CORNER. */
void nineSlice(gfx::Renderer& r, const gfx::Texture& tex, float x, float y,
               float w, float h, float c, u32 color) {
    const float sc = CORNER;              /* source corner in texels */
    const float sm = TILE - 2.f * sc;     /* source middle */
    const float mw = w - 2.f * c;         /* dest middle width */
    const float mh = h - 2.f * c;

    gfx::VertT* v = r.beginSprites(tex, 9);
    if (!v) return;
    int i = 0;
    auto emit = [&](float sx, float sy, float sw, float sh, float dx, float dy,
                    float dw, float dh) {
        if (dw <= 0.f || dh <= 0.f) return;
        v[i * 2 + 0] = {sx, sy, color, dx, dy, 0.f};
        v[i * 2 + 1] = {sx + sw, sy + sh, color, dx + dw, dy + dh, 0.f};
        i++;
    };
    emit(0, 0, sc, sc, x, y, c, c);
    emit(sc, 0, sm, sc, x + c, y, mw, c);
    emit(TILE - sc, 0, sc, sc, x + w - c, y, c, c);
    emit(0, sc, sc, sm, x, y + c, c, mh);
    emit(sc, sc, sm, sm, x + c, y + c, mw, mh);
    emit(TILE - sc, sc, sc, sm, x + w - c, y + c, c, mh);
    emit(0, TILE - sc, sc, sc, x, y + h - c, c, c);
    emit(sc, TILE - sc, sm, sc, x + c, y + h - c, mw, c);
    emit(TILE - sc, TILE - sc, sc, sc, x + w - c, y + h - c, c, c);
    r.endSprites(v, i);
}

}  // namespace

bool init() { return bakeMasks() && bakeSystemIcons(); }

void roundedRect(gfx::Renderer& r, float x, float y, float w, float h,
                 float radius, u32 color) {
    float c = rsClamp(radius, 1.f, float(CORNER));
    if (c * 2.f > w) c = w * 0.5f;
    if (c * 2.f > h) c = h * 0.5f;
    nineSlice(r, s_round, x, y, w, h, c, color);
}

void roundedOutline(gfx::Renderer& r, float x, float y, float w, float h,
                    float radius, u32 color) {
    float c = rsClamp(radius, 1.f, float(CORNER));
    if (c * 2.f > w) c = w * 0.5f;
    if (c * 2.f > h) c = h * 0.5f;
    nineSlice(r, s_roundRing, x, y, w, h, c, color);
}

void dropShadow(gfx::Renderer& r, float x, float y, float w, float h,
                float radius, u32 color) {
    roundedRect(r, x + 1.f, y + 2.f, w, h, radius, color);
}

void focusRow(gfx::Renderer& r, float x, float y, float w, float h,
              u32 fill, u32 bar, u32 shadow) {
    dropShadow(r, x, y, w, h, 8.f, shadow);
    roundedRect(r, x, y, w, h, 8.f, fill);
    roundedOutline(r, x, y, w, h, 8.f, bar);
}

void circle(gfx::Renderer& r, float cx, float cy, float radius, u32 color) {
    r.sprite(s_disc, 0, 0, TILE, TILE, cx - radius, cy - radius, radius * 2.f,
             radius * 2.f, color);
}

void ring(gfx::Renderer& r, float cx, float cy, float radius, u32 color) {
    r.sprite(s_discRing, 0, 0, TILE, TILE, cx - radius, cy - radius,
             radius * 2.f, radius * 2.f, color);
}

void iconClock(gfx::Renderer& r, float cx, float cy, float radius, u32 color) {
    ring(r, cx, cy, radius, color);
    const float th = radius > 9.f ? 2.f : 1.5f;
    r.line(cx, cy, cx, cy - radius * 0.55f, th, color);
    r.line(cx, cy, cx + radius * 0.42f, cy + radius * 0.18f, th, color);
}

void iconStar(gfx::Renderer& r, float cx, float cy, float radius, u32 color) {
    /* Five-point star as a triangle fan around the center. */
    constexpr int P = 5;
    float ox[P], oy[P], ix[P], iy[P];
    for (int i = 0; i < P; i++) {
        const float ao = -1.5707963f + 6.2831853f * float(i) / P;
        const float ai = ao + 6.2831853f / (2 * P);
        ox[i] = cx + std::cos(ao) * radius;
        oy[i] = cy + std::sin(ao) * radius;
        ix[i] = cx + std::cos(ai) * radius * 0.44f;
        iy[i] = cy + std::sin(ai) * radius * 0.44f;
    }
    for (int i = 0; i < P; i++) {
        const int j = (i + 1) % P;
        r.tri(cx, cy, ox[i], oy[i], ix[i], iy[i], color);
        r.tri(cx, cy, ix[i], iy[i], ox[j], oy[j], color);
    }
}

void iconGear(gfx::Renderer& r, float cx, float cy, float radius, u32 color) {
    ring(r, cx, cy, radius * 0.62f, color);
    for (int i = 0; i < 8; i++) {
        const float a = 6.2831853f * float(i) / 8.f;
        const float c = std::cos(a), s = std::sin(a);
        r.line(cx + c * radius * 0.62f, cy + s * radius * 0.62f,
               cx + c * radius, cy + s * radius, radius * 0.32f, color);
    }
}

void iconSystem(gfx::Renderer& r, int systemIdx, float x, float y, float size,
                u32 base, u32 detail) {
    systemIdx = rsClamp(systemIdx, 0, SYSTEM_COUNT - 1);
    x = float(int(x));
    y = float(int(y));
    size = size >= 48.f ? 64.f : 32.f;
    if (systemIdx == CONSOLE_COUNT) {
        iconGear(r, x + size * .5f, y + size * .5f, size * .3f, base);
        return;
    }
    (void)detail;
    const gfx::TexFilter previous = r.texFilter();
    r.setTexFilter(gfx::TexFilter::Nearest);
    const u32 tint = rsWithAlpha(rsHex(0xFFFFFF), rsAlphaOf(base));
    r.sprite(s_systemIcons[systemIdx], 0.f, 0.f, SYSTEM_CELL, SYSTEM_CELL,
             x, y, size, size, tint);
    r.setTexFilter(previous);
}

void buttonGlyph(gfx::Renderer& r, Button b, float cx, float cy, float radius,
                 u32 color) {
    const float k = radius * 0.62f;
    switch (b) {
        case Button::Cross:
            r.line(cx - k, cy - k, cx + k, cy + k, 2.f, color);
            r.line(cx - k, cy + k, cx + k, cy - k, 2.f, color);
            break;
        case Button::Circle:
            ring(r, cx, cy, radius * 0.92f, color);
            break;
        case Button::Triangle: {
            const float t = radius * 0.95f;
            const float x0 = cx, y0 = cy - t;
            const float x1 = cx - t * 0.87f, y1 = cy + t * 0.62f;
            const float x2 = cx + t * 0.87f, y2 = cy + t * 0.62f;
            r.line(x0, y0, x1, y1, 2.f, color);
            r.line(x1, y1, x2, y2, 2.f, color);
            r.line(x2, y2, x0, y0, 2.f, color);
            break;
        }
        case Button::Square: {
            const float s = radius * 0.78f;
            r.line(cx - s, cy - s, cx + s, cy - s, 2.f, color);
            r.line(cx + s, cy - s, cx + s, cy + s, 2.f, color);
            r.line(cx + s, cy + s, cx - s, cy + s, 2.f, color);
            r.line(cx - s, cy + s, cx - s, cy - s, 2.f, color);
            break;
        }
    }
}

void battery(gfx::Renderer& r, float x, float y, float level, bool charging,
             u32 color, u32 accent) {
    /* Deliberately rectilinear and integer-aligned. The previous 22x11
     * rounded mask was sampled across half pixels and looked blurry on an
     * IPS PSP-1000 panel. */
    x = float(int(x));
    y = float(int(y));
    constexpr float W = 18.f, H = 8.f;
    r.rect(x, y, W, 1.f, color);
    r.rect(x, y + H - 1.f, W, 1.f, color);
    r.rect(x, y + 1.f, 1.f, H - 2.f, color);
    r.rect(x + W - 1.f, y + 1.f, 1.f, H - 2.f, color);
    r.rect(x + W, y + 2.f, 2.f, H - 4.f, color);
    if (level >= 0.f) {
        const float fill =
            float(int(rsClamp(level, 0.f, 1.f) * (W - 4.f) + .5f));
        const u32 c = (charging || level > 0.25f) ? accent
                                                  : rsHex(0xE05252);
        if (fill >= 1.f) r.rect(x + 2.f, y + 2.f, fill, H - 4.f, c);
    } else {
        r.rect(x + 5.f, y + 3.f, W - 10.f, 1.f, color);
    }
    if (charging) {
        r.rect(x + 8.f, y + 2.f, 2.f, 2.f, rsHex(0xFFFFFF));
        r.rect(x + 7.f, y + 4.f, 2.f, 2.f, rsHex(0xFFFFFF));
    }
}

}  // namespace rs::ui::prim
