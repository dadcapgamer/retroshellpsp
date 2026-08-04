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

/* GU list exhaustion is silent on hardware and manifests as repeated or
 * corrupted sprites. The larger buffer is still tiny beside the PSP-1000
 * frontend budget and gives transitions/modal overlays a safe ceiling. */
constexpr int LIST_BYTES = 192 * 1024;
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
    m_drawBuffer = reinterpret_cast<void*>(vram::FB0_OFFSET);
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
    /* Ordered dithering is useful for photographs, but across flat UI
     * surfaces it creates an obvious grain pattern on modern IPS panels.
     * The frontend favors clean edges; box art remains naturally varied. */
    sceGuDisable(GU_DITHER);
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

    /* sceGuClear respects the active scissor rectangle. A clip left behind
     * by any previous UI path therefore clears only part of the next back
     * buffer, leaving old focus outlines, FPS glyphs and icons behind as
     * green/red trails. Reset the complete baseline before clearing rather
     * than relying on every caller to perfectly balance its clip calls. */
    sceGuScissor(0, 0, RS_SCREEN_W, RS_SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
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
    m_drawBuffer = sceGuSwapBuffers();
    /* Capture the buffer after it becomes the displayed frame. Reading the
     * pre-swap draw pointer is unreliable in PPSSPP and produced stale,
     * identical screenshots even while valid core frames were presented. */
    if (m_capturePath[0]) captureNow();
    if (!m_displayOn) {
        sceGuDisplay(GU_TRUE);
        m_displayOn = true;
    }
}

void Renderer::requestCapture(const char* path) {
    std::snprintf(m_capturePath, sizeof m_capturePath, "%s", path);
}

void Renderer::captureNow() {
    /* Prefer the actual displayed framebuffer. This remains correct if a GU
     * implementation uses a swap policy different from our local bookkeeping. */
    void* displayed = nullptr;
    int stride = vram::FB_STRIDE;
    int psm = GU_PSM_5650;
    if (sceDisplayGetFrameBuf(&displayed, &stride, &psm,
                              PSP_DISPLAY_SETBUF_IMMEDIATE) < 0 ||
        !displayed || psm != GU_PSM_5650) {
        displayed = m_drawBuffer;
        stride = vram::FB_STRIDE;
    }
    const uintptr_t draw = reinterpret_cast<uintptr_t>(displayed);
    const uintptr_t edram = reinterpret_cast<uintptr_t>(sceGeEdramGetAddr());
    constexpr uintptr_t EDRAM_BYTES = 2u * 1024u * 1024u;
    const uintptr_t addr = draw < EDRAM_BYTES ? edram + draw : draw;
    /* Force a GE readback into ordinary RAM. PPSSPP may keep an optimized
     * framebuffer only in its host renderer, in which case directly reading
     * either PSP VRAM alias returns an old image even after GU sync. */
    const size_t snapBytes = RS_SCREEN_W * RS_SCREEN_H * sizeof(u16);
    u16* snapshot = static_cast<u16*>(memalign(64, snapBytes));
    if (!snapshot) {
        m_capturePath[0] = 0;
        return;
    }
    std::memset(snapshot, 0, snapBytes);
    sceKernelDcacheWritebackInvalidateRange(snapshot, snapBytes);
    sceGuStart(GU_DIRECT, s_list);
    sceGuCopyImage(GU_PSM_5650, 0, 0, RS_SCREEN_W, RS_SCREEN_H, stride,
                   reinterpret_cast<void*>(addr),
                   0, 0, RS_SCREEN_W, snapshot);
    sceGuFinish();
    sceGuSync(0, 0);
    sceKernelDcacheInvalidateRange(snapshot, snapBytes);
    const u16* fb = snapshot;

    u8* rgb = static_cast<u8*>(std::malloc(RS_SCREEN_W * RS_SCREEN_H * 3));
    if (!rgb) {
        std::free(snapshot);
        m_capturePath[0] = 0;
        return;
    }
    for (int y = 0; y < RS_SCREEN_H; y++) {
        const u16* row = fb + y * RS_SCREEN_W;
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
    std::free(snapshot);
    m_capturePath[0] = 0;
}

void Renderer::setScissor(int x, int y, int w, int h) {
    const int x1 = rsClamp(x, 0, RS_SCREEN_W);
    const int y1 = rsClamp(y, 0, RS_SCREEN_H);
    const int x2 = rsClamp(x + rsClamp(w, 0, RS_SCREEN_W), x1, RS_SCREEN_W);
    const int y2 = rsClamp(y + rsClamp(h, 0, RS_SCREEN_H), y1, RS_SCREEN_H);
    sceGuScissor(x1, y1, x2, y2);
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
    /* Emulator BGR555 frames use the top bit as spare colour data, not
     * alpha. Ignore texture alpha for 5551 so the upstream PSP-native
     * Snes9x layout remains opaque; vertex alpha still controls overlays. */
    sceGuTexFunc(GU_TFX_MODULATE,
                 t->psm == GU_PSM_5551 ? GU_TCC_RGB : GU_TCC_RGBA);
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
    if (w <= 0 || h <= 0 || w > 512 || h > 512) {
        RS_LOGW("texture: invalid dimensions %dx%d", w, h);
        return false;
    }
    const int bpp = bytesPerPixel(psm);
    /* Pitch must be a 16-byte multiple for swizzling; height a multiple
     * of 8. pow2 with floors handles both. */
    const u32 minW = 16u / u32(bpp) < 4 ? 4 : 16u / u32(bpp);
    u32 texW = rsNextPow2(u32(w));
    u32 texH = rsNextPow2(u32(h));
    if (texW < minW) texW = minW;
    if (texH < 8) texH = 8;

    const u64 pitch64 = u64(texW) * u32(bpp);
    const u64 size64 = pitch64 * texH;
    if (pitch64 > UINT32_MAX || size64 > UINT32_MAX) return false;
    const u32 pitch = u32(pitch64);
    const u32 size  = u32(size64);

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
    if (!t.valid() || !pixels || pitchBytes <= 0) return;
    const int bpp = bytesPerPixel(t.psm);
    const u32 dstPitch = u32(t.texW) * u32(bpp);
    const u32 copyBytes = u32(t.width) * u32(bpp);
    if (u32(pitchBytes) < copyBytes || copyBytes > dstPitch) return;
    u8* dst = static_cast<u8*>(t.pixels);
    const u8* src = static_cast<const u8*>(pixels);
    for (int y = 0; y < t.height; y++)
        std::memcpy(dst + u32(y) * dstPitch, src + u32(y) * u32(pitchBytes),
                    copyBytes);
    sceKernelDcacheWritebackRange(t.pixels, dstPitch * t.texH);
}

void Renderer::freeTexture(Texture& t) {
    if (t.ownsRam && t.pixels) std::free(t.pixels);
    /* VRAM is reclaimed wholesale via vram::freeAll(). */
    t = Texture{};
}

}  // namespace rs::gfx
