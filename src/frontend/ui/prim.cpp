#include "frontend/ui/prim.h"
#include "runtime/log.h"

#include "rs_asset_game_boy_24_png.h"
#include "rs_asset_game_boy_color_24_png.h"
#include "rs_asset_game_boy_advance_24_png.h"
#include "rs_asset_nes_24_png.h"
#include "rs_asset_snes_24_png.h"
#include "rs_asset_megadrive_24_png.h"
#include "rs_asset_master_system_24_png.h"
#include "rs_asset_game_gear_24_png.h"
#include "rs_asset_pc_engine_24_png.h"
#include "rs_asset_game_boy_48_png.h"
#include "rs_asset_game_boy_64_png.h"
#include "rs_asset_game_boy_color_48_png.h"
#include "rs_asset_game_boy_color_64_png.h"
#include "rs_asset_game_boy_advance_48_png.h"
#include "rs_asset_game_boy_advance_64_png.h"
#include "rs_asset_nes_48_png.h"
#include "rs_asset_nes_64_png.h"
#include "rs_asset_snes_48_png.h"
#include "rs_asset_snes_64_png.h"
#include "rs_asset_megadrive_48_png.h"
#include "rs_asset_megadrive_64_png.h"
#include "rs_asset_master_system_48_png.h"
#include "rs_asset_master_system_64_png.h"
#include "rs_asset_game_gear_48_png.h"
#include "rs_asset_game_gear_64_png.h"
#include "rs_asset_pc_engine_48_png.h"
#include "rs_asset_pc_engine_64_png.h"
#include "rs_asset_control_cross_24_png.h"
#include "rs_asset_control_circle_24_png.h"
#include "rs_asset_control_triangle_24_png.h"
#include "rs_asset_control_square_24_png.h"
#include "rs_asset_control_start_24_png.h"
#include "rs_asset_control_left_shoulder_24_png.h"
#include "rs_asset_control_right_shoulder_24_png.h"
#include "rs_asset_control_select_24_png.h"
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
constexpr int DOT_TILE = 32;
constexpr int DOT_PITCH = 4;
gfx::Texture s_dots;       /* 32x32, one lit texel per 4x4 cell */
constexpr int SYSTEM_COUNT = 10;
constexpr int SYSTEM_CELL_SMALL = 24;    /* list-row glyph */
constexpr int SYSTEM_CELL = 48;          /* resting / list size */
constexpr int SYSTEM_CELL_LARGE = 64;    /* selected / preview size */
constexpr int CONSOLE_COUNT = SYSTEM_COUNT - 1;
constexpr int GLYPH_COUNT = 8;
/* Cross, Circle, Triangle, Square, Start, L1, R1 — cropped to their ink. */
gfx::Texture s_glyphs[GLYPH_COUNT];
gfx::Texture s_systemIconsSmall[CONSOLE_COUNT];
gfx::Texture s_systemIcons[CONSOLE_COUNT];
gfx::Texture s_systemIconsLarge[CONSOLE_COUNT];

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

    /* Dot lattice, offset every other row so it reads as a fine diagonal
     * weave rather than a grid. */
    std::memset(buf, 0, sizeof buf);
    for (int y = 0; y < DOT_TILE; y += DOT_PITCH / 2)
        for (int x = (y / (DOT_PITCH / 2)) % 2 ? DOT_PITCH / 2 : 0;
             x < DOT_TILE; x += DOT_PITCH)
            buf[y * DOT_TILE + x] = 255;
    if (!gfx::Renderer::createTexture(s_dots, DOT_TILE, DOT_TILE, GU_PSM_T8,
                                      buf, /*dynamic=*/true))
        return false;

    s_round.clut = s_roundRing.clut = s_disc.clut = s_discRing.clut =
        s_dots.clut = gfx::Renderer::alphaClut();
    return true;
}

bool bakeSystemIcons() {
    struct EmbeddedPng {
        const unsigned char* bytes;
        unsigned int length;
    };
    /* Pre-sized, hard-edged icons (assets/icons/consoles/<name>-24|48|64.png,
     * generated from the 192 px masters by tools/crisp_icons.py).
     * Keep this order identical to db::System. */
    const EmbeddedPng icons[CONSOLE_COUNT][3] = {
        {{rs_asset_game_boy_24_png, rs_asset_game_boy_24_png_len},
         {rs_asset_game_boy_48_png, rs_asset_game_boy_48_png_len},
         {rs_asset_game_boy_64_png, rs_asset_game_boy_64_png_len}},
        {{rs_asset_game_boy_color_24_png, rs_asset_game_boy_color_24_png_len},
         {rs_asset_game_boy_color_48_png, rs_asset_game_boy_color_48_png_len},
         {rs_asset_game_boy_color_64_png, rs_asset_game_boy_color_64_png_len}},
        {{rs_asset_game_boy_advance_24_png, rs_asset_game_boy_advance_24_png_len},
         {rs_asset_game_boy_advance_48_png, rs_asset_game_boy_advance_48_png_len},
         {rs_asset_game_boy_advance_64_png, rs_asset_game_boy_advance_64_png_len}},
        {{rs_asset_nes_24_png, rs_asset_nes_24_png_len},
         {rs_asset_nes_48_png, rs_asset_nes_48_png_len},
         {rs_asset_nes_64_png, rs_asset_nes_64_png_len}},
        {{rs_asset_snes_24_png, rs_asset_snes_24_png_len},
         {rs_asset_snes_48_png, rs_asset_snes_48_png_len},
         {rs_asset_snes_64_png, rs_asset_snes_64_png_len}},
        {{rs_asset_megadrive_24_png, rs_asset_megadrive_24_png_len},
         {rs_asset_megadrive_48_png, rs_asset_megadrive_48_png_len},
         {rs_asset_megadrive_64_png, rs_asset_megadrive_64_png_len}},
        {{rs_asset_master_system_24_png, rs_asset_master_system_24_png_len},
         {rs_asset_master_system_48_png, rs_asset_master_system_48_png_len},
         {rs_asset_master_system_64_png, rs_asset_master_system_64_png_len}},
        {{rs_asset_game_gear_24_png, rs_asset_game_gear_24_png_len},
         {rs_asset_game_gear_48_png, rs_asset_game_gear_48_png_len},
         {rs_asset_game_gear_64_png, rs_asset_game_gear_64_png_len}},
        {{rs_asset_pc_engine_24_png, rs_asset_pc_engine_24_png_len},
         {rs_asset_pc_engine_48_png, rs_asset_pc_engine_48_png_len},
         {rs_asset_pc_engine_64_png, rs_asset_pc_engine_64_png_len}},
    };

    for (int icon = 0; icon < CONSOLE_COUNT; ++icon) {
        gfx::Texture* targets[3] = {&s_systemIconsSmall[icon],
                                    &s_systemIcons[icon],
                                    &s_systemIconsLarge[icon]};
        const int cells[3] = {SYSTEM_CELL_SMALL, SYSTEM_CELL, SYSTEM_CELL_LARGE};
        /* Each system keeps its own texture per size: a shared atlas once
         * contaminated every console card with a neighbour's pixels on
         * hardware. Independent images also give exact texture bounds. */
        for (int v = 0; v < 3; ++v) {
            int w = 0, h = 0, comp = 0;
            stbi_uc* px = stbi_load_from_memory(
                icons[icon][v].bytes, int(icons[icon][v].length), &w, &h,
                &comp, 4);
            if (!px || w != cells[v] || h != cells[v]) {
                RS_LOGE("ui: console icon %d (%d px) is not a valid %dx%d "
                        "RGBA asset", icon, cells[v], cells[v], cells[v]);
                if (px) stbi_image_free(px);
                return false;
            }
            const bool ok = gfx::Renderer::createTexture(
                *targets[v], w, h, GU_PSM_8888, px, /*dynamic=*/true);
            stbi_image_free(px);
            if (!ok) return false;
        }
    }

    /* Control glyphs: 24 px Figma exports with black RGB and coverage alpha.
     * Recolour to white so they tint to any theme colour, and crop to the
     * ink so legend spacing follows the visible shape, not the padding. */
    const EmbeddedPng glyphs[GLYPH_COUNT] = {
        {rs_asset_control_cross_24_png,    rs_asset_control_cross_24_png_len},
        {rs_asset_control_circle_24_png,   rs_asset_control_circle_24_png_len},
        {rs_asset_control_triangle_24_png, rs_asset_control_triangle_24_png_len},
        {rs_asset_control_square_24_png,   rs_asset_control_square_24_png_len},
        {rs_asset_control_start_24_png,    rs_asset_control_start_24_png_len},
        {rs_asset_control_left_shoulder_24_png,
         rs_asset_control_left_shoulder_24_png_len},
        {rs_asset_control_right_shoulder_24_png,
         rs_asset_control_right_shoulder_24_png_len},
        {rs_asset_control_select_24_png,   rs_asset_control_select_24_png_len},
    };
    for (int i = 0; i < GLYPH_COUNT; ++i) {
        int w = 0, h = 0, comp = 0;
        stbi_uc* px = stbi_load_from_memory(glyphs[i].bytes,
                                            int(glyphs[i].length), &w, &h,
                                            &comp, 4);
        if (!px) {
            RS_LOGE("ui: control glyph %d failed to decode", i);
            return false;
        }
        int x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (px[(y * w + x) * 4 + 3] > 8) {
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
        if (x1 < x0) { stbi_image_free(px); return false; }
        const int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
        u8 crop[24 * 24 * 4] = {};
        if (cw > 24 || ch > 24) { stbi_image_free(px); return false; }
        for (int y = 0; y < ch; ++y)
            for (int x = 0; x < cw; ++x) {
                const u8* src = px + ((y0 + y) * w + x0 + x) * 4;
                u8* dst = crop + (y * cw + x) * 4;
                dst[0] = dst[1] = dst[2] = 255;
                dst[3] = src[3];
            }
        stbi_image_free(px);
        if (!gfx::Renderer::createTexture(s_glyphs[i], cw, ch, GU_PSM_8888,
                                          crop, /*dynamic=*/true))
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

void outlineRect(gfx::Renderer& r, float x, float y, float w, float h,
                 float t, u32 color) {
    x = float(int(x)); y = float(int(y));
    w = float(int(w)); h = float(int(h));
    r.rect(x, y, w, t, color);
    r.rect(x, y + h - t, w, t, color);
    r.rect(x, y + t, t, h - 2.f * t, color);
    r.rect(x + w - t, y + t, t, h - 2.f * t, color);
}

void chevron(gfx::Renderer& r, Dir d, float cx, float cy, float size,
             float thickness, u32 color) {
    float dx = 0.f, dy = 0.f;
    switch (d) {
        case Dir::Left:  dx = -1.f; break;
        case Dir::Right: dx =  1.f; break;
        case Dir::Up:    dy = -1.f; break;
        case Dir::Down:  dy =  1.f; break;
    }
    /* Tip points along (dx,dy); the two arms fall back and spread. */
    const float tipX = cx + dx * size * .5f, tipY = cy + dy * size * .5f;
    const float backX = cx - dx * size * .5f, backY = cy - dy * size * .5f;
    const float px = -dy, py = dx;   /* perpendicular */
    r.line(tipX, tipY, backX + px * size, backY + py * size, thickness, color);
    r.line(tipX, tipY, backX - px * size, backY - py * size, thickness, color);
}

void arrow(gfx::Renderer& r, Dir d, float cx, float cy, float size,
           u32 color) {
    const float dy = d == Dir::Down ? 1.f : -1.f;
    r.line(cx, cy - dy * size, cx, cy + dy * size, 2.f, color);
    chevron(r, d, cx, cy + dy * size * .55f, size * .7f, 2.f, color);
}

void dotField(gfx::Renderer& r, float x, float y, float w, float h,
              u32 color) {
    x = float(int(x)); y = float(int(y));
    w = float(int(w)); h = float(int(h));
    const gfx::TexFilter previous = r.texFilter();
    r.setTexFilter(gfx::TexFilter::Nearest);
    for (float ty = 0.f; ty < h; ty += float(DOT_TILE)) {
        const float th = h - ty < float(DOT_TILE) ? h - ty : float(DOT_TILE);
        for (float tx = 0.f; tx < w; tx += float(DOT_TILE)) {
            const float tw =
                w - tx < float(DOT_TILE) ? w - tx : float(DOT_TILE);
            r.sprite(s_dots, 0.f, 0.f, tw, th, x + tx, y + ty, tw, th, color);
        }
    }
    r.setTexFilter(previous);
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
    const int cell = size >= 56.f ? SYSTEM_CELL_LARGE
                   : size >= 36.f ? SYSTEM_CELL : SYSTEM_CELL_SMALL;
    size = float(cell);
    if (systemIdx == CONSOLE_COUNT) {
        iconGear(r, x + size * .5f, y + size * .5f, size * .3f, base);
        return;
    }
    (void)detail;
    const gfx::TexFilter previous = r.texFilter();
    r.setTexFilter(gfx::TexFilter::Nearest);
    const u32 tint = rsWithAlpha(rsHex(0xFFFFFF), rsAlphaOf(base));
    const gfx::Texture& texture = cell == SYSTEM_CELL_LARGE
        ? s_systemIconsLarge[systemIdx]
        : cell == SYSTEM_CELL ? s_systemIcons[systemIdx]
                              : s_systemIconsSmall[systemIdx];
    const float sourceSize = float(cell);
    r.sprite(texture, 0.f, 0.f, sourceSize, sourceSize,
             x, y, size, size, tint);
    r.setTexFilter(previous);
}

namespace {
int glyphIndex(Button b) {
    switch (b) {
        case Button::Cross:    return 0;
        case Button::Circle:   return 1;
        case Button::Triangle: return 2;
        case Button::Square:   return 3;
        case Button::Start:    return 4;
        case Button::L1:       return 5;
        case Button::R1:       return 6;
        case Button::Select:   return 7;
        default:               return -1;
    }
}
}  // namespace

float buttonGlyphWidth(Button b) {
    const int i = glyphIndex(b);
    if (b == Button::DpadLeftRight) return 22.f;
    return i >= 0 ? float(s_glyphs[i].width) : 13.f;
}

void buttonGlyph(gfx::Renderer& r, Button b, float cx, float cy, float radius,
                 u32 color) {
    const int index = glyphIndex(b);
    if (b == Button::DpadLeftRight) {
        /* Two horizontal stem arrows: arrow() only draws vertical ones. */
        const float x0 = float(int(cx));
        const float y0 = float(int(cy));
        r.line(x0 - 10.f, y0, x0 - 2.f, y0, 2.f, color);
        chevron(r, Dir::Left, x0 - 8.f, y0, 6.f, 2.f, color);
        r.line(x0 + 2.f, y0, x0 + 10.f, y0, 2.f, color);
        chevron(r, Dir::Right, x0 + 8.f, y0, 6.f, 2.f, color);
        return;
    }
    if (index < 0) {
        arrow(r, b == Button::DpadUp ? Dir::Up : Dir::Down, cx, cy,
              radius * 0.8f, color);
        return;
    }
    const gfx::Texture& t = s_glyphs[index];
    const gfx::TexFilter previous = r.texFilter();
    r.setTexFilter(gfx::TexFilter::Nearest);
    const float w = float(t.width), h = float(t.height);
    r.sprite(t, 0.f, 0.f, w, h, float(int(cx - w * .5f + .5f)),
             float(int(cy - h * .5f + .5f)), w, h, color);
    r.setTexFilter(previous);
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
