#include "frontend/app.h"
#include "frontend/autopilot.h"
#include "frontend/scenes/boot_scene.h"
#include "platform/psp/power.h"
#include "runtime/log.h"

#include <pspkernel.h>

#include <cmath>
#include <cstdio>

/* Baked font atlases embedded at build time (see cmake/rs_assets.cmake). */
#include "rs_asset_font_title_rsf.h"
#include "rs_asset_font_large_rsf.h"
#include "rs_asset_font_body_rsf.h"
#include "rs_asset_font_small_rsf.h"

/* Set by the HOME-menu exit callback in main.cpp. */
extern volatile bool g_exitRequested;

namespace rs {

bool App::init() {
    if (!m_renderer.init()) return false;
    if (!ui::prim::init()) {
        RS_LOGE("app: primitive bake failed");
        return false;
    }

    struct { text::Font* font; const unsigned char* data; unsigned len; } fonts[] = {
        {&m_fonts.title, rs_asset_font_title_rsf, rs_asset_font_title_rsf_len},
        {&m_fonts.large, rs_asset_font_large_rsf, rs_asset_font_large_rsf_len},
        {&m_fonts.body,  rs_asset_font_body_rsf,  rs_asset_font_body_rsf_len},
        {&m_fonts.small, rs_asset_font_small_rsf, rs_asset_font_small_rsf_len},
    };
    for (auto& f : fonts) {
        if (!f.font->load(f.data, f.len)) {
            RS_LOGE("app: font load failed");
            return false;
        }
    }

    m_pad.init();
    m_pal = m_darkTheme ? theme::dark() : theme::light();
    m_themeFrom = m_pal;
    m_lastUs = sceKernelGetSystemTimeLow();

    switchScene(std::make_unique<BootScene>(), /*instant=*/true);
    RS_LOGI("app: init complete");
    return true;
}

void App::shutdown() {
    m_scene.reset();
    m_pending.reset();
    m_fonts.title.unload();
    m_fonts.large.unload();
    m_fonts.body.unload();
    m_fonts.small.unload();
    m_renderer.shutdown();
}

void App::setTheme(bool dark) {
    if (dark == m_darkTheme) return;
    m_themeFrom = m_pal;
    m_darkTheme = dark;
    m_themeFade.start(0.3f);
}

void App::switchScene(std::unique_ptr<Scene> next, bool instant) {
    if (instant) {
        m_scene = std::move(next);
        m_scene->enter(*this);
        m_pending.reset();
        m_fadingOut = false;
        return;
    }
    m_pending = std::move(next);
    m_fadingOut = true;
    m_sceneFade.start(0.15f);
}

void App::run() {
    while (!g_exitRequested) {
        const u32 now = sceKernelGetSystemTimeLow();
        float dt = float(now - m_lastUs) * 1e-6f;
        m_lastUs = now;
        dt = rsClamp(dt, 0.f, 0.05f);   /* suspend/resume safety */

#ifdef RS_AUTOPILOT
        autopilot::tick(*this);
#endif
        m_pad.poll();
        update(dt);
        draw();
    }
}

void App::update(float dt) {
    m_time += dt;

    /* Theme crossfade. */
    if (m_themeFade.running()) {
        const float t = ui::easeInOutQuad(m_themeFade.update(dt));
        m_pal = theme::blend(m_themeFrom,
                             m_darkTheme ? theme::dark() : theme::light(), t);
    }

    /* Scene transition: fade out, swap, fade back in. */
    if (m_sceneFade.running()) {
        m_sceneFade.update(dt);
        if (!m_sceneFade.running() && m_fadingOut && m_pending) {
            m_scene = std::move(m_pending);
            m_scene->enter(*this);
            m_fadingOut = false;
            m_sceneFade.start(0.15f);
        }
    }

    /* Battery/clock polling is cheap but not free — every ~2s. */
    if (--m_batteryPoll <= 0) {
        m_batteryPoll = 120;
        m_batteryPct = power::batteryPercent();
        m_batteryChg = power::batteryCharging();
    }

#ifdef RS_DEBUG_OVERLAY
    if (m_pad.isPressed(PSP_CTRL_TRIANGLE) && m_pad.isHeld(PSP_CTRL_LTRIGGER))
        m_showOverlay = !m_showOverlay;
#endif

    if (m_scene && !m_fadingOut) m_scene->update(*this, dt);
}

void App::draw() {
    m_renderer.beginFrame(m_pal.bgBottom);
    if (m_scene) m_scene->draw(*this);

    /* Scene-transition scrim on top of everything. */
    if (m_sceneFade.running() || m_fadingOut) {
        const float t = m_sceneFade.t;
        const float a = m_fadingOut ? t : 1.f - t;
        m_renderer.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H,
                        rsWithAlpha(m_pal.scrim, u32(a * 255.f)));
    }

#ifdef RS_DEBUG_OVERLAY
    if (m_showOverlay) drawDebugOverlay();
#endif
    m_renderer.endFrame();
}

/* ---------------------------------------------------------------------- */
/* Shared chrome                                                          */
/* ---------------------------------------------------------------------- */

void App::drawWave(float baseY, float amp, float freq, float speed,
                   float phase, float height, u32 color) {
    constexpr int COLS = 25;
    constexpr float STEP = float(RS_SCREEN_W) / float(COLS - 1);
    gfx::VertC* v = m_renderer.allocVertsC(COLS * 2);
    if (!v) return;
    const u32 bottom = rsWithAlpha(color, 0);
    for (int i = 0; i < COLS; i++) {
        const float x = float(i) * STEP;
        const float y = baseY +
            std::sin(x * 0.013f * freq + m_time * speed + phase) * amp +
            std::sin(x * 0.031f * freq - m_time * speed * 0.6f + phase * 2.f) *
                amp * 0.35f;
        v[i * 2 + 0] = {color, x, y, 0.f};
        v[i * 2 + 1] = {bottom, x, y + height, 0.f};
    }
    m_renderer.drawStripC(v, COLS * 2);
}

void App::drawBackground() {
    m_renderer.rectV(0, 0, RS_SCREEN_W, RS_SCREEN_H, m_pal.bgTop,
                     m_pal.bgBottom);
    drawWave(158.f, 16.f, 1.0f, 0.45f, 0.0f, 130.f, m_pal.waveA);
    drawWave(186.f, 12.f, 1.4f, 0.32f, 2.1f, 100.f, m_pal.waveB);
}

void App::drawTopBar() {
    const auto& f = m_fonts;
    f.small.draw(m_renderer, 14.f, 8.f, "RetroSuite", m_pal.textDim);

    /* Clock, right-aligned, with battery pill to its right. */
    int hh = 0, mm = 0;
    power::clockNow(hh, mm);
    char clock[8];
    std::snprintf(clock, sizeof clock, "%d:%02d", hh, mm);
    f.small.draw(m_renderer, RS_SCREEN_W - 46.f, 8.f, clock, m_pal.textSecondary,
                 text::Align::Right);
    ui::prim::battery(m_renderer, RS_SCREEN_W - 38.f, 9.f,
                      m_batteryPct < 0 ? -1.f : float(m_batteryPct) / 100.f,
                      m_batteryChg, m_pal.textSecondary, m_pal.accent);
}

void App::drawHintBar(const Hint* hints, int count) {
    float x = RS_SCREEN_W - 14.f;
    for (int i = count - 1; i >= 0; i--) {
        const float w = m_fonts.small.measure(hints[i].label);
        x -= w;
        m_fonts.small.draw(m_renderer, x, RS_SCREEN_H - 20.f, hints[i].label,
                           m_pal.textSecondary);
        x -= 12.f;
        ui::prim::buttonGlyph(m_renderer, hints[i].button, x,
                              RS_SCREEN_H - 13.f, 6.f, m_pal.textSecondary);
        x -= 18.f;
    }
}

#ifdef RS_DEBUG_OVERLAY
void App::drawDebugOverlay() {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.1f fps  %.2f ms", m_renderer.fps(),
                  m_renderer.frameMs());
    ui::prim::roundedRect(m_renderer, 8.f, 24.f, 118.f, 18.f, 6.f,
                          rsHex(0x000000, 140));
    m_fonts.small.draw(m_renderer, 14.f, 27.f, buf, rsHex(0x7CFF9B));
}
#endif

}  // namespace rs
