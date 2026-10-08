#include "frontend/scenes/boot_scene.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "runtime/config.h"

#include "frontend/splash.h"
#include "stb_image.h"

#include <pspgu.h>

#include <cmath>
#include <cstring>

namespace rs {

namespace {
constexpr float FADE_IN  = 0.45f;
constexpr float HOLD_END = 1.15f;
}  // namespace

BootScene::~BootScene() { gfx::Renderer::freeTexture(m_logo); }

void BootScene::enter(App& app) {
    /* App::init has already presented the exact pre-baked splash while the
     * remaining services load, so continue fully visible without flashing
     * back through a second fade-in. Drawing that same plate (rather than
     * re-typesetting it) keeps the hand-off pixel-identical and needs no
     * display-size font atlas in the EBOOT. It is held as RGB565 — half the
     * memory — for the second it is on screen. */
    m_t = FADE_IN;
    m_handedOff = false;
    gfx::Renderer::freeTexture(m_logo);

    int w = 0, h = 0;
    stbi_uc* pixels = splash::compose(app.pal(), w, h);
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
    r.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H, app.pal().bg);
    if (m_logo.valid())
        r.sprite(m_logo, 0, 0, RS_SCREEN_W, RS_SCREEN_H, 0, 0, RS_SCREEN_W,
                 RS_SCREEN_H, rsHex(0xFFFFFF));
}

}  // namespace rs
