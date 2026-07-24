#include "frontend/app.h"
#include "frontend/autopilot.h"
#include "frontend/game_session.h"
#include "frontend/scenes/boot_scene.h"
#include "platform/psp/audio_out.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/power.h"
#include "platform/psp/vram.h"
#include "runtime/arena.h"
#include "runtime/config.h"
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
    if (!audio::init()) RS_LOGW("app: audio unavailable");

    cfg::load();
    power::setCpuMhz(cfg::get().cpuMenuMhz);

    /* Fonts and primitive masks stay resident across core launches;
     * everything allocated after this mark is evictable. */
    gfx::vram::setBootMark();

    m_theme = theme::loadTheme(cfg::get().theme);
    m_pal = m_theme.palette;
    m_themeFrom = m_pal;

    m_library.load();
    if (!m_index.loadCache()) RS_LOGI("index: no cache yet");
    m_scanner.start();

    m_lastUs = sceKernelGetSystemTimeLow();
    switchScene(std::make_unique<BootScene>(), /*instant=*/true);
    RS_LOGI("app: init complete");
    return true;
}

void App::launchGame(const db::GameEntry& game) {
    RS_LOGI("app: launching '%s'", game.name.c_str());
    switchScene(std::make_unique<GameSession>(game));
}

void App::evictForCore() {
    m_boxart.clear();
    m_theme.freeAssets();
    gfx::vram::freeToBootMark();
    RS_LOGI("app: evicted frontend caches (%u KB arena, %u KB vram free)",
            unsigned(mem::available() / 1024),
            unsigned(gfx::vram::available() / 1024));
}

void App::restoreAfterCore() {
    m_theme = theme::loadTheme(m_theme.id);
    m_pal = m_theme.palette;
    m_themeFrom = m_pal;
    RS_LOGI("app: frontend restored");
}

void App::shutdown() {
    m_scene.reset();
    m_pending.reset();
    m_boxart.clear();
    m_theme.freeAssets();
    audio::shutdown();
    m_fonts.title.unload();
    m_fonts.large.unload();
    m_fonts.body.unload();
    m_fonts.small.unload();
    m_renderer.shutdown();
}

void App::setThemeById(const std::string& id) {
    if (id == m_theme.id) return;
    m_themeFrom = m_pal;
    m_theme.freeAssets();
    m_boxart.clear();   /* texture VRAM may be reshuffled by new theme */
    m_theme = theme::loadTheme(id);
    m_themeFade.start(0.3f);
    cfg::get().theme = m_theme.id;
    cfg::save();
}

void App::toggleTheme() {
    setThemeById(darkTheme() ? "light" : "dark");
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

void App::toast(const char* msg) {
    std::snprintf(m_toastMsg, sizeof m_toastMsg, "%s", msg);
    m_toastTween.start(2.4f);
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
    /* Persist small state on the way out. */
    m_library.save();
}

void App::update(float dt) {
    m_time += dt;

    /* Theme crossfade. */
    if (m_themeFade.running()) {
        const float t = ui::easeInOutQuad(m_themeFade.update(dt));
        m_pal = theme::blend(m_themeFrom, m_theme.palette, t);
    } else {
        m_pal = m_theme.palette;
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

    /* Fold in finished scans. */
    std::vector<db::GameEntry> results;
    if (m_scanner.takeResults(results)) {
        m_index.replaceAll(std::move(results));
        m_index.saveCache();
        RS_LOGI("index: refreshed, %d games", m_index.totalCount());
    }

    /* Battery/clock polling is cheap but not free — every ~2s. */
    if (--m_batteryPoll <= 0) {
        m_batteryPoll = 120;
        m_batteryPct = power::batteryPercent();
        m_batteryChg = power::batteryCharging();
    }

    m_toastTween.update(dt);

#ifdef RS_DEBUG_OVERLAY
    if (m_pad.isPressed(PSP_CTRL_TRIANGLE) && m_pad.isHeld(PSP_CTRL_LTRIGGER))
        m_showOverlay = !m_showOverlay;
#endif

    if (m_scene && !m_fadingOut) m_scene->update(*this, dt);
}

void App::draw() {
    m_renderer.beginFrame(m_pal.bgBottom);
    if (m_scene) m_scene->draw(*this);

    drawScanStatus();
    drawToast();

    /* Scene-transition scrim on top of everything. */
    if (m_sceneFade.running() || m_fadingOut) {
        const float t = m_sceneFade.t;
        const float a = m_fadingOut ? t : 1.f - t;
        m_renderer.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H,
                        rsWithAlpha(m_pal.scrim, u32(a * 255.f)));
    }

#ifdef RS_DEBUG_OVERLAY
    if (m_showOverlay || cfg::get().showFps) drawDebugOverlay();
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
    if (m_theme.background.valid()) {
        m_renderer.sprite(m_theme.background, 0, 0,
                          m_theme.background.width, m_theme.background.height,
                          0, 0, RS_SCREEN_W, RS_SCREEN_H, rsHex(0xFFFFFF));
    } else {
        m_renderer.rectV(0, 0, RS_SCREEN_W, RS_SCREEN_H, m_pal.bgTop,
                         m_pal.bgBottom);
    }
    if (m_theme.waves) {
        drawWave(158.f, 16.f, 1.0f, 0.45f, 0.0f, 130.f, m_pal.waveA);
        drawWave(186.f, 12.f, 1.4f, 0.32f, 2.1f, 100.f, m_pal.waveB);
    }
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

void App::drawToast() {
    if (!m_toastTween.running() || !m_toastMsg[0]) return;
    const float t = m_toastTween.t;
    /* Quick fade in, hold, fade out. */
    float a = 1.f;
    if (t < 0.1f) a = t / 0.1f;
    else if (t > 0.8f) a = (1.f - t) / 0.2f;
    const u32 alpha = u32(a * 235.f);

    const float w = m_fonts.body.measure(m_toastMsg) + 36.f;
    const float x = (RS_SCREEN_W - w) / 2.f;
    const float y = RS_SCREEN_H - 46.f;
    ui::prim::roundedRect(m_renderer, x, y, w, 26.f, 12.f,
                          rsWithAlpha(darkTheme() ? rsHex(0x2A3242)
                                                  : rsHex(0x353C4A),
                                      alpha));
    m_fonts.body.draw(m_renderer, RS_SCREEN_W / 2.f, y + 4.f, m_toastMsg,
                      rsWithAlpha(rsHex(0xF2F5FA), alpha),
                      text::Align::Center);
}

void App::drawScanStatus() {
    if (!m_scanner.running()) return;
    /* Spinner: orbiting dot. */
    const float cx = 22.f, cy = RS_SCREEN_H - 14.f;
    ui::prim::ring(m_renderer, cx, cy, 7.f, rsWithAlpha(m_pal.textDim, 120));
    const float a = m_time * 5.f;
    ui::prim::circle(m_renderer, cx + std::cos(a) * 7.f,
                     cy + std::sin(a) * 7.f, 2.2f, m_pal.accent);
    char buf[40];
    std::snprintf(buf, sizeof buf, "Scanning… %d", m_scanner.progress());
    m_fonts.small.draw(m_renderer, cx + 14.f, cy - 7.f, buf, m_pal.textDim);
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
