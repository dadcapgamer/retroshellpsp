#include "frontend/text/font.h"
#include "frontend/text/utf8.h"
#include "runtime/log.h"

#include <pspgu.h>

#include <cstdlib>
#include <cstring>

namespace rs::text {

namespace {
constexpr u32 RSF_MAGIC = 0x31465352u; /* "RSF1" */

struct __attribute__((packed)) Header {
    u32 magic;
    u16 atlasW, atlasH;
    s16 ascent, descent, lineHeight;
    u16 glyphCount, reserved;
};
static_assert(sizeof(Header) == 18, "rsf header layout");
}  // namespace

bool Font::load(const void* data, u32 size) {
    if (size < sizeof(Header)) return false;
    Header h;
    std::memcpy(&h, data, sizeof h);
    if (h.magic != RSF_MAGIC) {
        RS_LOGE("font: bad magic");
        return false;
    }
    if (!h.atlasW || !h.atlasH || h.atlasW > 512 || h.atlasH > 512)
        return false;
    const u64 glyphBytes64 = u64(h.glyphCount) * sizeof(Glyph);
    const u64 atlasBytes64 = u64(h.atlasW) * u64(h.atlasH);
    const u64 required = sizeof(Header) + glyphBytes64 + atlasBytes64;
    if (required > size || glyphBytes64 > UINT32_MAX) {
        RS_LOGE("font: truncated (%u < %u)", unsigned(size),
                unsigned(required > UINT32_MAX ? UINT32_MAX : required));
        return false;
    }
    const u32 glyphBytes = u32(glyphBytes64);

    m_glyphs = static_cast<Glyph*>(std::malloc(glyphBytes));
    if (!m_glyphs) return false;
    const u8* p = static_cast<const u8*>(data) + sizeof(Header);
    std::memcpy(m_glyphs, p, glyphBytes);
    m_glyphCount = h.glyphCount;
    m_ascent     = h.ascent;
    m_descent    = h.descent;
    m_lineHeight = h.lineHeight;

    const u8* atlas = p + glyphBytes;
    if (!gfx::Renderer::createTexture(m_tex, h.atlasW, h.atlasH, GU_PSM_T8,
                                      atlas)) {
        std::free(m_glyphs);
        m_glyphs = nullptr;
        return false;
    }
    m_tex.clut = gfx::Renderer::alphaClut();
    return true;
}

void Font::unload() {
    gfx::Renderer::freeTexture(m_tex);
    std::free(m_glyphs);
    m_glyphs = nullptr;
    m_glyphCount = 0;
}

const Font::Glyph* Font::find(u32 cp) const {
    /* Glyphs are stored sorted by codepoint. */
    u32 lo = 0, hi = m_glyphCount;
    while (lo < hi) {
        const u32 mid = (lo + hi) / 2;
        if (m_glyphs[mid].cp < cp) lo = mid + 1;
        else hi = mid;
    }
    if (lo < m_glyphCount && m_glyphs[lo].cp == cp) return &m_glyphs[lo];
    return nullptr;
}

float Font::measure(const char* text) const {
    float w = 0.f;
    while (*text) {
        const u32 cp = utf8Next(&text);
        const Glyph* g = find(cp);
        if (!g) g = find('?');
        if (g) w += float(g->xadv);
    }
    return w;
}

void Font::draw(gfx::Renderer& r, float x, float y, const char* text,
                u32 color, Align align) const {
    if (!m_glyphs || !text || !*text) return;

    if (align == Align::Center)     x -= measure(text) * 0.5f;
    else if (align == Align::Right) x -= measure(text);
    x = float(int(x));           /* pixel-snap for crisp glyphs */
    const float baseline = float(int(y)) + float(m_ascent);

    /* Count renderable glyphs, then fill one sprite batch. */
    int count = 0;
    for (const char* s = text; *s;) {
        const u32 cp = utf8Next(&s);
        const Glyph* g = find(cp);
        if (!g) g = find('?');
        if (g && g->w) count++;
    }
    if (count == 0) return;

    gfx::VertT* v = r.beginSprites(m_tex, count);
    if (!v) return;
    int i = 0;
    float pen = x;
    for (const char* s = text; *s;) {
        const u32 cp = utf8Next(&s);
        const Glyph* g = find(cp);
        if (!g) g = find('?');
        if (!g) continue;
        if (g->w) {
            const float gx = pen + float(g->xoff);
            const float gy = baseline + float(g->yoff);
            v[i * 2 + 0] = {float(g->x), float(g->y), color, gx, gy, 0.f};
            v[i * 2 + 1] = {float(g->x + g->w), float(g->y + g->h), color,
                            gx + float(g->w), gy + float(g->h), 0.f};
            i++;
        }
        pen += float(g->xadv);
    }
    r.endSprites(v, i);
}

void Font::drawShadow(gfx::Renderer& r, float x, float y, const char* text,
                      u32 color, u32 shadowColor, Align align) const {
    draw(r, x, y + 1.f, text, shadowColor, align);
    draw(r, x, y, text, color, align);
}

}  // namespace rs::text
