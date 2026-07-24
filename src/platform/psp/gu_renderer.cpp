#include "platform/psp/gu_renderer.h"
#include "platform/psp/vram.h"
#include "runtime/log.h"

#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include "stb_image_write.h"

namespace rs::gfx {

namespace {

constexpr int LIST_BYTES = 96 * 1024;
alignas(64) u8 s_list[LIST_BYTES];

alignas(64) u32 s_alphaClut[256];
bool s_clutReady = false;

constexpr int VTYPE_T =
    GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D;
constexpr int VTYPE_C = GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D;

int bytesPerPixel(int psm) {
    switch (psm) {
        case GU_PSM_T8:   return 1;
        case GU_PSM_8888: return 4;
        default:          return 2;  /* 5650 / 5551 / 4444 */
    }
}

/* Swizzle a linear image into the GE's 16x8-byte block layout. `pitch`
 * must be a multiple of 16 bytes and `height` a multiple of 8 rows —
 * guaranteed by the pow2 padding in createTexture. */
void swizzle(u8* dst, const u8* src, int pitch, int height) {
    const int rowBlocks = pitch / 16;
    for (int by = 0; by < height / 8; by++) {
        for (int bx = 0; bx < rowBlocks; bx++) {
            for (int row = 0; row < 8; row++) {
                std::memcpy(dst, src + ((by * 8 + row) * pitch) + bx * 16, 16);
                dst += 16;
            }
        }
    }
}

}  // namespace

const u32* Renderer::alphaClut() {
    if (!s_clutReady) {
        for (u32 i = 0; i < 256; i++) s_alphaClut[i] = (i << 24) | 0x00FFFFFFu;
        sceKernelDcacheWritebackRange(s_alphaClut, sizeof(s_alphaClut));
        s_clutReady = true;
    }
    return s_alphaClut;
}

bool Renderer::init() {
    vram::init();

    sceGuInit();
    sceGuStart(GU_DIRECT, s_list);
    sceGuDrawBuffer(GU_PSM_5650, reinterpret_cast<void*>(vram::FB0_OFFSET),
                    vram::FB_STRIDE);
    sceGuDispBuffer(RS_SCREEN_W, RS_SCREEN_H,
                    reinterpret_cast<void*>(vram::FB1_OFFSET), vram::FB_STRIDE);
    sceGuOffset(2048 - RS_SCREEN_W / 2, 2048 - RS_SCREEN_H / 2);
    sceGuViewport(2048, 2048, RS_SCREEN_W, RS_SCREEN_H);
    sceGuScissor(0, 0, RS_SCREEN_W, RS_SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDepthMask(GU_TRUE);           /* no depth writes — no Z buffer */
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuEnable(GU_DITHER);            /* smooths gradients on 5650 */
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    return true;
}

void Renderer::shutdown() {
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void Renderer::beginFrame(u32 clearColor) {
    m_frameStart = sceKernelGetSystemTimeLow();
    sceGuStart(GU_DIRECT, s_list);
    sceGuClearColor(clearColor);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    m_bound = nullptr;
    m_texturing = false;
    sceGuDisable(GU_TEXTURE_2D);
}

void Renderer::endFrame() {
    sceKernelDcacheWritebackAll();  /* vertices written this frame */
    sceGuFinish();
    sceGuSync(0, 0);

    if (m_capturePath[0]) captureNow();

    const u32 busyEnd = sceKernelGetSystemTimeLow();
    m_frameMs = float(busyEnd - m_frameStart) * 0.001f;
    if (m_lastFrameStart != 0) {
        const float total = float(m_frameStart - m_lastFrameStart) * 0.001f;
        if (total > 0.f) {
            const float inst = 1000.f / total;
            m_fps += (inst - m_fps) * 0.1f;  /* smoothed */
        }
    }
    m_lastFrameStart = m_frameStart;

    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    m_drawIsFb1 = !m_drawIsFb1;
    if (!m_displayOn) {
        sceGuDisplay(GU_TRUE);
        m_displayOn = true;
    }
}

void Renderer::requestCapture(const char* path) {
    std::snprintf(m_capturePath, sizeof m_capturePath, "%s", path);
}

void Renderer::captureNow() {
    /* The GE has just finished the current draw buffer; read it through the
     * uncached mirror so we see the GPU's writes. */
    const u32 fbOff = m_drawIsFb1 ? vram::FB1_OFFSET : vram::FB0_OFFSET;
    const u8* base = static_cast<const u8*>(sceGeEdramGetAddr()) + fbOff;
    const u16* fb = reinterpret_cast<const u16*>(
        reinterpret_cast<uintptr_t>(base) | 0x40000000u);

    u8* rgb = static_cast<u8*>(std::malloc(RS_SCREEN_W * RS_SCREEN_H * 3));
    if (!rgb) {
        m_capturePath[0] = 0;
        return;
    }
    for (int y = 0; y < RS_SCREEN_H; y++) {
        const u16* row = fb + y * vram::FB_STRIDE;
        u8* out = rgb + y * RS_SCREEN_W * 3;
        for (int x = 0; x < RS_SCREEN_W; x++) {
            const u16 v = row[x];
            out[x * 3 + 0] = u8((v & 0x1F) << 3);
            out[x * 3 + 1] = u8(((v >> 5) & 0x3F) << 2);
            out[x * 3 + 2] = u8(((v >> 11) & 0x1F) << 3);
        }
    }
    stbi_write_png(m_capturePath, RS_SCREEN_W, RS_SCREEN_H, 3, rgb,
                   RS_SCREEN_W * 3);
    std::free(rgb);
    m_capturePath[0] = 0;
}

void Renderer::setScissor(int x, int y, int w, int h) {
    sceGuScissor(rsClamp(x, 0, RS_SCREEN_W), rsClamp(y, 0, RS_SCREEN_H),
                 rsClamp(w, 0, RS_SCREEN_W), rsClamp(h, 0, RS_SCREEN_H));
}

void Renderer::resetScissor() { sceGuScissor(0, 0, RS_SCREEN_W, RS_SCREEN_H); }

void Renderer::flushStateFor(bool textured) {
    if (textured == m_texturing) return;
    if (textured) sceGuEnable(GU_TEXTURE_2D);
    else          sceGuDisable(GU_TEXTURE_2D);
    m_texturing = textured;
}

void Renderer::bind(const Texture* t) {
    flushStateFor(t != nullptr);
    if (!t || t == m_bound) return;

    if (t->psm == GU_PSM_T8) {
        sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
        sceGuClutLoad(256 / 8, t->clut ? t->clut : alphaClut());
    }
    sceGuTexMode(t->psm, 0, 0, t->swizzled ? 1 : 0);
    sceGuTexImage(0, t->texW, t->texH, t->texW, t->pixels);
    const int f = (m_filter == TexFilter::Linear) ? GU_LINEAR : GU_NEAREST;
    sceGuTexFilter(f, f);
    sceGuTexFlush();
    m_bound = t;
}

/* --- solid geometry ---------------------------------------------------- */

void Renderer::rect(float x, float y, float w, float h, u32 c) {
    bind(nullptr);
    auto* v = static_cast<VertC*>(sceGuGetMemory(2 * sizeof(VertC)));
    v[0] = {c, x, y, 0.f};
    v[1] = {c, x + w, y + h, 0.f};
    sceGuDrawArray(GU_SPRITES, VTYPE_C, 2, nullptr, v);
}

void Renderer::rectV(float x, float y, float w, float h, u32 top, u32 bottom) {
    bind(nullptr);
    auto* v = static_cast<VertC*>(sceGuGetMemory(4 * sizeof(VertC)));
    v[0] = {top, x, y, 0.f};
    v[1] = {top, x + w, y, 0.f};
    v[2] = {bottom, x, y + h, 0.f};
    v[3] = {bottom, x + w, y + h, 0.f};
    sceGuDrawArray(GU_TRIANGLE_STRIP, VTYPE_C, 4, nullptr, v);
}

void Renderer::rectH(float x, float y, float w, float h, u32 left, u32 right) {
    bind(nullptr);
    auto* v = static_cast<VertC*>(sceGuGetMemory(4 * sizeof(VertC)));
    v[0] = {left, x, y, 0.f};
    v[1] = {right, x + w, y, 0.f};
    v[2] = {left, x, y + h, 0.f};
    v[3] = {right, x + w, y + h, 0.f};
    sceGuDrawArray(GU_TRIANGLE_STRIP, VTYPE_C, 4, nullptr, v);
}

void Renderer::line(float x1, float y1, float x2, float y2, float th, u32 c) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = dx * dx + dy * dy;
    if (len <= 0.f) return;
    len = __builtin_sqrtf(len);
    const float nx = -dy / len * th * 0.5f;
    const float ny = dx / len * th * 0.5f;

    bind(nullptr);
    auto* v = static_cast<VertC*>(sceGuGetMemory(4 * sizeof(VertC)));
    v[0] = {c, x1 + nx, y1 + ny, 0.f};
    v[1] = {c, x1 - nx, y1 - ny, 0.f};
    v[2] = {c, x2 + nx, y2 + ny, 0.f};
    v[3] = {c, x2 - nx, y2 - ny, 0.f};
    sceGuDrawArray(GU_TRIANGLE_STRIP, VTYPE_C, 4, nullptr, v);
}

void Renderer::tri(float x0, float y0, float x1, float y1, float x2, float y2,
                   u32 c) {
    bind(nullptr);
    auto* v = static_cast<VertC*>(sceGuGetMemory(3 * sizeof(VertC)));
    v[0] = {c, x0, y0, 0.f};
    v[1] = {c, x1, y1, 0.f};
    v[2] = {c, x2, y2, 0.f};
    sceGuDrawArray(GU_TRIANGLES, VTYPE_C, 3, nullptr, v);
}

VertC* Renderer::allocVertsC(int n) {
    bind(nullptr);
    return static_cast<VertC*>(sceGuGetMemory(n * int(sizeof(VertC))));
}

void Renderer::drawStripC(VertC* v, int n) {
    sceGuDrawArray(GU_TRIANGLE_STRIP, VTYPE_C, n, nullptr, v);
}

void Renderer::drawTrisC(VertC* v, int n) {
    sceGuDrawArray(GU_TRIANGLES, VTYPE_C, n, nullptr, v);
}

/* --- textured sprites --------------------------------------------------- */

void Renderer::sprite(const Texture& t, float sx, float sy, float sw, float sh,
                      float dx, float dy, float dw, float dh, u32 tint) {
    bind(&t);
    auto* v = static_cast<VertT*>(sceGuGetMemory(2 * sizeof(VertT)));
    v[0] = {sx, sy, tint, dx, dy, 0.f};
    v[1] = {sx + sw, sy + sh, tint, dx + dw, dy + dh, 0.f};
    sceGuDrawArray(GU_SPRITES, VTYPE_T, 2, nullptr, v);
}

VertT* Renderer::beginSprites(const Texture& t, int spriteCount) {
    bind(&t);
    return static_cast<VertT*>(
        sceGuGetMemory(spriteCount * 2 * int(sizeof(VertT))));
}

void Renderer::endSprites(VertT* verts, int spriteCount) {
    if (spriteCount <= 0) return;
    sceGuDrawArray(GU_SPRITES, VTYPE_T, spriteCount * 2, nullptr, verts);
}

/* --- textures ------------------------------------------------------------ */

bool Renderer::createTexture(Texture& out, int w, int h, int psm,
                             const void* pixels, bool dynamic) {
    const int bpp = bytesPerPixel(psm);
    /* Pitch must be a 16-byte multiple for swizzling; height a multiple
     * of 8. pow2 with floors handles both. */
    const u32 minW = 16u / u32(bpp) < 4 ? 4 : 16u / u32(bpp);
    u32 texW = rsNextPow2(u32(w));
    u32 texH = rsNextPow2(u32(h));
    if (texW < minW) texW = minW;
    if (texH < 8) texH = 8;

    const u32 pitch = texW * u32(bpp);
    const u32 size  = pitch * texH;

    /* Stage the padded linear image, duplicating the last row/column into
     * the padding so linear filtering never bleeds garbage. */
    u8* staging = static_cast<u8*>(std::malloc(size));
    if (!staging) return false;
    std::memset(staging, 0, size);
    const u8* src = static_cast<const u8*>(pixels);
    const u32 rowBytes = u32(w) * u32(bpp);
    if (src) {
        for (int y = 0; y < h; y++) {
            u8* dst = staging + u32(y) * pitch;
            std::memcpy(dst, src + u32(y) * rowBytes, rowBytes);
            if (u32(w) < texW)
                std::memcpy(dst + rowBytes, dst + rowBytes - bpp, u32(bpp));
        }
        if (u32(h) < texH)
            std::memcpy(staging + u32(h) * pitch,
                        staging + u32(h - 1) * pitch, pitch);
    }

    void* final = nullptr;
    bool inVram = false, ownsRam = false;
    if (!dynamic) {
        final = vram::alloc(size);
        inVram = final != nullptr;
    }
    if (!final) {
        final = memalign(16, size);
        ownsRam = final != nullptr;
        if (!final) {
            std::free(staging);
            return false;
        }
    }

    if (dynamic) {
        std::memcpy(final, staging, size);
    } else {
        swizzle(static_cast<u8*>(final), staging, int(pitch), int(texH));
    }
    std::free(staging);
    sceKernelDcacheWritebackRange(final, size);

    out.pixels   = final;
    out.width    = u16(w);
    out.height   = u16(h);
    out.texW     = u16(texW);
    out.texH     = u16(texH);
    out.psm      = u8(psm);
    out.swizzled = !dynamic;
    out.inVram   = inVram;
    out.ownsRam  = ownsRam;
    return true;
}

void Renderer::updateTexture(Texture& t, const void* pixels, int pitchBytes) {
    const int bpp = bytesPerPixel(t.psm);
    const u32 dstPitch = u32(t.texW) * u32(bpp);
    u8* dst = static_cast<u8*>(t.pixels);
    const u8* src = static_cast<const u8*>(pixels);
    for (int y = 0; y < t.height; y++)
        std::memcpy(dst + u32(y) * dstPitch, src + u32(y) * u32(pitchBytes),
                    u32(t.width) * u32(bpp));
    sceKernelDcacheWritebackRange(t.pixels, dstPitch * t.texH);
}

void Renderer::freeTexture(Texture& t) {
    if (t.ownsRam && t.pixels) std::free(t.pixels);
    /* VRAM is reclaimed wholesale via vram::freeAll(). */
    t = Texture{};
}

}  // namespace rs::gfx
