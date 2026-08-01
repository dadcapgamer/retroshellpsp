/** PSP GU renderer — the only place draw commands are issued.
 *
 * 480x272, 16-bit (5650) double-buffered output, vblank-synced. Pure 2D:
 * no depth buffer, painter's order, GU_TRANSFORM_2D vertices (screen-space
 * coordinates, texel UVs). Frontend code draws through this interface only;
 * no sceGu* calls exist outside this file.
 */
#pragma once

#include "rs_common.h"

namespace rs::gfx {

/* A GPU-ready texture. Static UI textures are swizzled and live in VRAM
 * when it fits; dynamic textures (emulator output) stay linear in RAM so
 * the CPU can rewrite them every frame. */
struct Texture {
    void*      pixels   = nullptr;
    const u32* clut     = nullptr;  /* 256-entry palette for T8 textures */
    u16 width = 0, height = 0;      /* logical size */
    u16 texW = 0, texH = 0;         /* pow2 size given to the GE */
    u8   psm      = 3;              /* GU_PSM_* value */
    bool swizzled = false;
    bool inVram   = false;
    bool ownsRam  = false;

    bool valid() const { return pixels != nullptr; }
};

/* Textured, colored 2D vertex (GU_TEXTURE_32BITF|GU_COLOR_8888|
 * GU_VERTEX_32BITF|GU_TRANSFORM_2D). UVs are in texels. */
struct VertT {
    float u, v;
    u32   color;
    float x, y, z;
};

/* Color-only 2D vertex. */
struct VertC {
    u32   color;
    float x, y, z;
};

enum class TexFilter { Nearest, Linear };

class Renderer {
public:
    bool init();
    void shutdown();

    void beginFrame(u32 clearColor);
    void endFrame();            /* finish, vblank, swap */

    /* Encode the frame being finished by the NEXT endFrame() to a PNG on
     * the Memory Stick (also used by the save-state thumbnail path). */
    void requestCapture(const char* path);

    float frameMs() const { return m_frameMs; }
    float fps() const     { return m_fps; }

    void setScissor(int x, int y, int w, int h);
    void resetScissor();

    /* --- solid geometry ------------------------------------------------ */
    void rect(float x, float y, float w, float h, u32 c);
    void rectV(float x, float y, float w, float h, u32 top, u32 bottom);
    void rectH(float x, float y, float w, float h, u32 left, u32 right);
    void line(float x1, float y1, float x2, float y2, float th, u32 c);
    void tri(float x0, float y0, float x1, float y1, float x2, float y2, u32 c);
    /* Caller-filled color-only vertices for meshes (wave background). */
    VertC* allocVertsC(int n);
    void   drawStripC(VertC* v, int n);
    void   drawTrisC(VertC* v, int n);

    /* --- textured sprites ----------------------------------------------- */
    void sprite(const Texture& t,
                float sx, float sy, float sw, float sh,
                float dx, float dy, float dw, float dh, u32 tint);
    /* Batched path (text): one allocation + one draw for many quads. */
    VertT* beginSprites(const Texture& t, int spriteCount);
    void   endSprites(VertT* verts, int spriteCount);

    void setTexFilter(TexFilter f) {
        if (m_filter == f) return;
        m_filter = f;
        /* Filtering is GE bind state. Force the current texture to be
         * rebound even when the caller draws the same atlas again. */
        m_bound = nullptr;
    }
    TexFilter texFilter() const { return m_filter; }

    /* --- textures -------------------------------------------------------- */
    /* `pixels` is tightly packed w*h at `psm` depth. Static textures are
     * swizzled and preferentially placed in VRAM. T8 textures must set
     * `clut` afterwards (or use alphaClut()). */
    static bool createTexture(Texture& out, int w, int h, int psm,
                              const void* pixels, bool dynamic = false);
    static void updateTexture(Texture& t, const void* pixels, int pitchBytes);
    static void freeTexture(Texture& t);

    /* Shared 256-entry white-with-alpha-ramp palette used by fonts and
     * procedural masks. */
    static const u32* alphaClut();

private:
    void bind(const Texture* t);
    void flushStateFor(bool textured);
    void captureNow();

    const Texture* m_bound   = nullptr;
    bool  m_texturing        = false;
    TexFilter m_filter       = TexFilter::Linear;
    bool  m_displayOn        = false;
    /* sceGuSwapBuffers() is the source of truth. GU implementations may
     * return either an EDRAM-relative offset or an absolute PSP VRAM
     * address, so captureNow() normalizes this value before reading it. */
    void* m_drawBuffer       = nullptr;
    float m_frameMs          = 0.f;
    float m_fps              = 0.f;
    u32   m_lastFrameStart   = 0;
    u32   m_frameStart       = 0;
    char  m_capturePath[128] = {};
};

}  // namespace rs::gfx
