/** Bitmap font renderer for .rsf atlases (baked by tools/assetgen.c).
 *
 * Atlases are 8-bit coverage uploaded as T8 textures with the shared
 * white-alpha CLUT, so text is tinted by vertex color at zero extra cost.
 */
#pragma once

#include "platform/psp/gu_renderer.h"
#include "rs_common.h"

namespace rs::text {

enum class Align { Left, Center, Right };

class Font {
public:
    /* Parses an .rsf image (e.g. an embedded asset). The glyph table is
     * copied; `data` may be discarded afterwards. */
    bool load(const void* data, u32 size);
    void unload();

    float measure(const char* text) const;
    float ascent() const     { return float(m_ascent); }
    float lineHeight() const { return float(m_lineHeight); }
    /* Height of a capital and its offset below the line-box top, measured
     * from 'H' at load. Layout centres labels on their caps, not on the line
     * box, so a row's text sits optically centred whatever the face. */
    float capHeight() const  { return float(m_capHeight); }
    /* Line-box top that centres caps in [boxY, boxY+boxH], pixel-snapped. */
    float centerY(float boxY, float boxH) const {
        return float(int(boxY + (boxH - float(m_capHeight)) * .5f + .5f)) -
               float(m_capTop);
    }

    /* (x,y) is the TOP-left of the line box. */
    void draw(gfx::Renderer& r, float x, float y, const char* text, u32 color,
              Align align = Align::Left) const;
    /* One-pixel faux bold: the pixel face has a single weight, so titles and
     * selected rows double-strike one pixel to the right. */
    void drawBold(gfx::Renderer& r, float x, float y, const char* text,
                  u32 color, Align align = Align::Left) const {
        draw(r, x, y, text, color, align);
        draw(r, x + 1.f, y, text, color, align);
    }
    /* Soft drop shadow first, then the text — for text over imagery. */
    void drawShadow(gfx::Renderer& r, float x, float y, const char* text,
                    u32 color, u32 shadowColor,
                    Align align = Align::Left) const;

private:
    struct Glyph {
        u32 cp;
        u16 x, y, w, h;
        s16 xoff, yoff, xadv, _pad;
    };

    const Glyph* find(u32 cp) const;

    gfx::Texture m_tex;
    Glyph* m_glyphs   = nullptr;
    u32 m_glyphCount  = 0;
    s16 m_ascent      = 0;
    s16 m_descent     = 0;
    s16 m_lineHeight  = 0;
    s16 m_capHeight   = 0;
    s16 m_capTop      = 0;
};

}  // namespace rs::text
