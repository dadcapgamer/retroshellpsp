#include "frontend/scenes/boot_scene.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"

namespace rs {

namespace {
constexpr float FADE_IN  = 0.45f;
constexpr float HOLD_END = 1.15f;
}

void BootScene::enter(App&) { m_t = 0.f; m_handedOff = false; }

void BootScene::update(App& app, float dt) {
    m_t += dt;
    if (m_t >= HOLD_END && !m_handedOff) {
        m_handedOff = true;
        app.switchScene(std::make_unique<HomeScene>());
    }
}

void BootScene::draw(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    app.drawBackground();

    const float k = ui::easeOutQuint(rsClamp(m_t / FADE_IN, 0.f, 1.f));
    const u32 alpha = u32(k * 255.f);
    const float rise = (1.f - k) * 14.f;

    app.fonts().title.draw(r, RS_SCREEN_W / 2.f, 108.f + rise, "RetroSuite",
                           rsWithAlpha(pal.textPrimary, alpha),
                           text::Align::Center);
    app.fonts().small.draw(r, RS_SCREEN_W / 2.f, 142.f + rise,
                           "ALL-IN-ONE RETRO EMULATION",
                           rsWithAlpha(pal.textDim, alpha),
                           text::Align::Center);

    /* Accent underline sweeps in. */
    const float w = 120.f * k;
    r.rect(RS_SCREEN_W / 2.f - w / 2.f, 138.f + rise, w, 2.f,
           rsWithAlpha(pal.accent, alpha));
}

}  // namespace rs
