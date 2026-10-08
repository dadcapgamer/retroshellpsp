#include "frontend/scenes/boot_scene.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "runtime/config.h"

#include "rs_asset_splash_png.h"
#include "stb_image.h"

#include <pspgu.h>

#include <cmath>
#include <cstring>

namespace rs {

namespace {
constexpr float FADE_IN  = 0.45f;
constexpr float HOLD_END = 1.15f;
constexpr float SUBTITLE_Y = 181.f;
constexpr float SUBTITLE_TRACKING = .2f;
constexpr float SHIMMER_CYCLE = .72f;

float trackedWidth(const text::Font& font, const char* value,
                   float tracking) {
    const int count = int(std::strlen(value));
    float width = count > 1 ? tracking * float(count - 1) : 0.f;
    for (int i = 0; i < count; i++) {
        const char glyph[2] = {value[i], '\0'};
        width += font.measure(glyph);
    }
    return width;
}

void drawTracked(gfx::Renderer& r, const text::Font& font, float centerX,
                 float y, const char* value, float tracking, u32 first,
                 u32 last, u32 alpha) {
    const int count = int(std::strlen(value));
    const float width = trackedWidth(font, value, tracking);
    float x = centerX - width * .5f;
    for (int i = 0; i < count; i++) {
        char glyph[2] = {value[i], '\0'};
        const float t = count > 1 ? float(i) / float(count - 1) : 0.f;
        font.draw(r, x, y, glyph,
                  rsWithAlpha(rsLerpColor(first, last, t), alpha));
        x += font.measure(glyph) + tracking;
    }
}

void drawSubtitleShimmer(gfx::Renderer& r, const text::Font& font,
                         float centerX, float elapsed, u32 alpha) {
    constexpr const char* VALUE = "PSP Retro Emulation";
    const u32 base = rsHex(0xD79A2B);
    const u32 shine = rsHex(0xFFE6A3);
    drawTracked(r, font, centerX, SUBTITLE_Y, VALUE, SUBTITLE_TRACKING,
                base, base, alpha);

    const float width = trackedWidth(font, VALUE, SUBTITLE_TRACKING);
    const float phase = std::fmod(rsClamp(elapsed, 0.f, 1000.f) /
                                  SHIMMER_CYCLE, 1.f);
    const float center = centerX - width * .5f - 12.f +
                         phase * (width + 24.f);
    static const int OFFSETS[] = {-10, -6, -2, 2, 6};
    static const u32 STRENGTH[] = {45, 105, 210, 105, 45};
    for (int i = 0; i < 5; ++i) {
        r.setScissor(int(center) + OFFSETS[i], int(SUBTITLE_Y - 1.f), 4, 16);
        drawTracked(r, font, centerX, SUBTITLE_Y, VALUE, SUBTITLE_TRACKING,
                    shine, shine, alpha * STRENGTH[i] / 255u);
    }
    r.resetScissor();
}
}

BootScene::~BootScene() { gfx::Renderer::freeTexture(m_logo); }

void BootScene::enter(App&) {
    /* App::init has already presented the exact pre-baked splash while the
     * remaining services load, so continue fully visible without flashing
     * back through a second fade-in. Drawing that same plate (rather than
     * re-typesetting it) keeps the hand-off pixel-identical and needs no
     * display-size font atlas in the EBOOT. It is held as RGB565 — half the
     * memory — for the second it is on screen. */
    m_t = FADE_IN;
    m_handedOff = false;
    gfx::Renderer::freeTexture(m_logo);

    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        rs_asset_splash_png, int(rs_asset_splash_png_len), &w, &h, &channels, 4);
    if (pixels && w == RS_SCREEN_W && h == RS_SCREEN_H) {
        u16* px565 = reinterpret_cast<u16*>(pixels);   /* in place, front to back */
        for (int i = 0; i < w * h; i++) {
            const stbi_uc* p = pixels + i * 4;
            px565[i] = u16(((p[2] >> 3) << 11) | ((p[1] >> 2) << 5) | (p[0] >> 3));
        }
        gfx::Renderer::createTexture(m_logo, w, h, GU_PSM_5650, px565,
                                     /*dynamic=*/true);
    }
    if (pixels) stbi_image_free(pixels);
}

void BootScene::update(App& app, float dt) {
    m_t += dt;
    if (m_t >= HOLD_END && !m_handedOff) {
        m_handedOff = true;
        /* Straight to Home, first boot included: emulators are chosen,
         * tuned or turned off per system in Settings > Systems. */
        app.switchScene(std::make_unique<HomeScene>());
    }
}

void BootScene::draw(App& app) {
    auto& r = app.renderer();
    r.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H, rsHex(0xFAF5EE));
    if (m_logo.valid())
        r.sprite(m_logo, 0, 0, RS_SCREEN_W, RS_SCREEN_H, 0, 0, RS_SCREEN_W,
                 RS_SCREEN_H, rsHex(0xFFFFFF));
    /* Replace the baked subtitle with the live one, as the startup plate
     * does, then let the shimmer run across it. */
    r.rect(0.f, SUBTITLE_Y - 5.f, RS_SCREEN_W, 22.f, rsHex(0xFAF5EE));
    drawSubtitleShimmer(r, app.fonts().small, RS_SCREEN_W / 2.f,
                        rsClamp(m_t - FADE_IN, 0.f, 1000.f), 255u);
}

}  // namespace rs
