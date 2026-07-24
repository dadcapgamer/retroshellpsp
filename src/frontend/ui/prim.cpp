#include "frontend/ui/prim.h"
#include "runtime/log.h"

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
    if (!gfx::Renderer::createTexture(s_round, TILE, TILE, GU_PSM_T8, buf))
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
    if (!gfx::Renderer::createTexture(s_roundRing, TILE, TILE, GU_PSM_T8, buf))
        return false;

    /* Disc + ring. */
    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++) {
            const float d = std::sqrt(std::pow(float(x) + .5f - 16.f, 2.f) +
                                      std::pow(float(y) + .5f - 16.f, 2.f));
            buf[y * TILE + x] = u8(255.f * rsClamp(15.5f - d, 0.f, 1.f));
        }
    if (!gfx::Renderer::createTexture(s_disc, TILE, TILE, GU_PSM_T8, buf))
        return false;

    for (int y = 0; y < TILE; y++)
        for (int x = 0; x < TILE; x++) {
            const float d = std::sqrt(std::pow(float(x) + .5f - 16.f, 2.f) +
                                      std::pow(float(y) + .5f - 16.f, 2.f));
            const float cov = rsClamp(15.5f - d, 0.f, 1.f) -
                              rsClamp(13.2f - d, 0.f, 1.f);
            buf[y * TILE + x] = u8(255.f * rsClamp(cov, 0.f, 1.f));
        }
    if (!gfx::Renderer::createTexture(s_discRing, TILE, TILE, GU_PSM_T8, buf))
        return false;

    s_round.clut = s_roundRing.clut = s_disc.clut = s_discRing.clut =
        gfx::Renderer::alphaClut();
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

bool init() { return bakeMasks(); }

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
    constexpr float W = 22.f, H = 11.f;
    roundedOutline(r, x, y, W, H, 3.f, color);
    r.rect(x + W + 1.f, y + 3.f, 2.f, H - 6.f, color);  /* terminal nub */
    if (level >= 0.f) {
        const float fill = rsClamp(level, 0.f, 1.f) * (W - 5.f);
        const u32 c = (charging || level > 0.25f) ? accent
                                                  : rsHex(0xE05252);
        if (fill >= 1.f) roundedRect(r, x + 2.5f, y + 2.5f, fill, H - 5.f, 2.f, c);
    } else {
        r.line(x + 5.f, y + H * 0.5f, x + W - 5.f, y + H * 0.5f, 1.5f, color);
    }
    if (charging) {
        /* Small bolt. */
        const float bx = x + W * 0.5f, by = y + H * 0.5f;
        r.tri(bx + 2.5f, by - 4.f, bx - 3.f, by + 1.f, bx + 0.5f, by + 1.f,
              rsHex(0xFFFFFF));
        r.tri(bx - 2.5f, by + 4.f, bx + 3.f, by - 1.f, bx - 0.5f, by - 1.f,
              rsHex(0xFFFFFF));
    }
}

}  // namespace rs::ui::prim
