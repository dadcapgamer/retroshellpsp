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
#include <pspgu.h>

#include <cmath>
#include <cstdio>

#include "stb_image.h"

/* Baked font atlases embedded at build time (see cmake/rs_assets.cmake). */
#include "rs_asset_font_title_rsf.h"
#include "rs_asset_font_large_rsf.h"
#include "rs_asset_font_body_rsf.h"
#include "rs_asset_font_small_rsf.h"
#include "rs_asset_splash_png.h"

/* Set by the HOME-menu exit callback in main.cpp. */
extern volatile bool g_exitRequested;

namespace rs {

namespace {

bool drawStartupPlate(gfx::Renderer& renderer) {
    int w = 0, h = 0, channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        rs_asset_splash_png, int(rs_asset_splash_png_len),
        &w, &h, &channels, 4);
    gfx::Texture splash;
    const bool ready = pixels && w == RS_SCREEN_W && h == RS_SCREEN_H &&
        gfx::Renderer::createTexture(
            splash, w, h, GU_PSM_8888, pixels, /*dynamic=*/false);
    if (pixels) stbi_image_free(pixels);

    renderer.beginFrame(rsHex(0xFAF5EE));
    if (ready)
        renderer.sprite(splash, 0, 0, w, h, 0, 0,
                        RS_SCREEN_W, RS_SCREEN_H, rsHex(0xFFFFFF));
    renderer.endFrame();

    /* This is the first and only texture allocated before the persistent UI
     * atlases. The GE has finished reading it, so reclaim its temporary VRAM
     * immediately and let primitive/font initialization start from a clean
     * cursor. */
    gfx::Renderer::freeTexture(splash);
    gfx::vram::freeAll();
    return ready;
}

}  // namespace

bool App::init() {
    const u32 initStart = sceKernelGetSystemTimeLow();
    if (!m_renderer.init()) return false;

    /* Present branding before Memory Stick logging, UI atlas creation,
     * library loading, core discovery, or scanner startup. The framebuffer
     * remains visible while those slower operations complete. */
    const bool startupPlateReady = drawStartupPlate(m_renderer);
    const u32 firstFrameUs = sceKernelGetSystemTimeLow() - initStart;
    const fs::RootMigration migration = fs::migrateLegacyRoot();
    log::init(/*toFile=*/true);
    RS_LOGI("RetroShell starting");
    if (migration != fs::RootMigration::None) {
        const char* result = migration == fs::RootMigration::Renamed
            ? "renamed"
            : (migration == fs::RootMigration::Merged ? "merged" : "FAILED");
        RS_LOGI("storage: legacy RETROSUITE migration %s", result);
    }
    RS_LOGI("arena: reserved %u KB (startup telemetry)",
            static_cast<unsigned>(mem::totalSize() / 1024));
    RS_LOGI("startup: first splash frame %u ms (%s)",
            unsigned(firstFrameUs / 1000),
            startupPlateReady ? "branded" : "solid fallback");

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
    /* User-mode PSP applications cannot portably query the chassis model,
     * but the constraint we care about is directly measurable: a PSP-1000
     * yields roughly a 17 MB arena, while later 64 MB models yield far more.
     * Leave generous separation from both values. */
    const bool lowMemoryHardware = mem::totalSize() < 24u * 1024u * 1024u;
    cfg::applyHardwareDefaults(lowMemoryHardware);
    RS_LOGI("hardware: %u KB core arena, PSP-1000 Safe Mode %s",
            unsigned(mem::totalSize() / 1024),
            cfg::get().psp1000SafeMode ? "on" : "off");
    power::setCpuMhz(cfg::get().cpuMenuMhz);

    /* Fonts and primitive masks stay resident across core launches;
     * everything allocated after this mark is evictable. */
    gfx::vram::setBootMark();

    m_theme = theme::loadTheme(cfg::get().theme);
    m_pal = theme::personalize(m_theme.palette, cfg::get().accent);
    m_themeFrom = m_pal;

    m_library.load();
    const bool libraryCached = m_index.loadCache();
    if (!libraryCached) RS_LOGI("index: no cache yet");
    m_cores.discover();
    if (!libraryCached || m_index.totalCount() == 0) {
        m_scanner.start();
    } else {
        RS_LOGI("scanner: using cached library; manual rescan available");
    }

    m_lastUs = sceKernelGetSystemTimeLow();
    switchScene(std::make_unique<BootScene>(), /*instant=*/true);
    RS_LOGI("app: init complete in %u ms",
            unsigned((sceKernelGetSystemTimeLow() - initStart) / 1000));
    return true;
}

void App::launchGame(const db::GameEntry& game, const CoreInfo* core) {
    if (!core) core = m_cores.resolve(game);
    if (!core) {
        char msg[64];
        std::snprintf(msg, sizeof msg, "No core installed for %s",
                      db::systemInfo(game.system).displayName);
        toast(msg);
        RS_LOGW("app: %s", msg);
        return;
    }
    RS_LOGI("app: launching '%s' via %s", game.name.c_str(),
            core->name.c_str());
    switchScene(std::make_unique<GameSession>(game, core->name));
}

void App::evictForCore() {
    /* A scanner competes for the 4 MB newlib heap and Memory Stick while a
     * core is loading. Stop and join it before changing the memory map. */
    m_scanner.stop();
    m_boxart.clear();
    m_theme.freeAssets();
    gfx::vram::freeToBootMark();
    RS_LOGI("app: evicted frontend caches (%u KB arena, %u KB vram free)",
            unsigned(mem::available() / 1024),
            unsigned(gfx::vram::available() / 1024));
}

void App::restoreAfterCore() {
    m_theme = theme::loadTheme(m_theme.id);
    m_pal = theme::personalize(m_theme.palette, cfg::get().accent);
    m_themeFrom = m_pal;
    RS_LOGI("app: frontend restored");
}

void App::shutdown() {
    m_scanner.stop();
    if (m_scene) m_scene->shutdown(*this);
    /* GameSession launch bookkeeping happens in the scene hook above, so
     * save the library afterwards. This also makes HOME-button exits while
     * a game is running retain their play statistics. */
    m_library.save();
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

void App::setAccentIndex(int index) {
    index = rsClamp(index, 0, theme::ACCENT_COUNT - 1);
    if (index == cfg::get().accent) return;
    m_themeFrom = m_pal;
    cfg::get().accent = index;
    m_themeFade.start(0.22f);
    cfg::save();
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
        if (cfg::get().uiSounds && audio::isPaused()) {
            const u32 pressed = m_pad.pressed();
            if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN |
                           PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
                audio::playUiSound(audio::UiSound::Move);
            } else if (pressed & PSP_CTRL_CIRCLE) {
                audio::playUiSound(audio::UiSound::Back);
            } else if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_SQUARE |
                                  PSP_CTRL_TRIANGLE | PSP_CTRL_START)) {
                audio::playUiSound(audio::UiSound::Confirm);
            }
        }
        update(dt);
        draw();
    }
}

void App::update(float dt) {
    m_time += dt;

    /* Theme crossfade. */
    if (m_themeFade.running()) {
        const float t = ui::easeInOutQuad(m_themeFade.update(dt));
        m_pal = theme::blend(
            m_themeFrom,
            theme::personalize(m_theme.palette, cfg::get().accent), t);
    } else {
        m_pal = theme::personalize(m_theme.palette, cfg::get().accent);
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
        if (!m_index.saveCache())
            RS_LOGW("index: failed to save cache for %d games",
                    m_index.totalCount());
        /* A rescan is also the explicit refresh path for artwork copied
         * beside existing ROMs while the frontend is already running.
         * Clear positive and negative entries so the next visible frame
         * re-resolves the sibling/legacy paths. */
        m_boxart.clear();
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
    } else if (m_theme.id == "dark") {
        /* The built-in dark theme is intentionally one uninterrupted field.
         * Do not apply the ambient wash or watermark used by other themes. */
        m_renderer.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H, m_pal.bgTop);
        return;
    } else {
        m_renderer.rectV(0, 0, RS_SCREEN_W, RS_SCREEN_H, m_pal.bgTop,
                         m_pal.bgBottom);
    }
    if (m_theme.waves) {
        /* Explicit custom-theme compatibility. Built-in themes use the
         * quieter static treatment below. */
        drawWave(158.f, 16.f, 1.0f, 0.45f, 0.0f, 130.f, m_pal.waveA);
        drawWave(186.f, 12.f, 1.4f, 0.32f, 2.1f, 100.f, m_pal.waveB);
    } else if (!m_theme.background.valid()) {
        /* A restrained ambient wash replaces the animated wave pattern.
         * It is static, cheap, and keeps the center of the screen quiet. */
        const u32 wash = rsWithAlpha(m_pal.accent, m_pal.dark ? 8u : 5u);
        m_renderer.rectH(0.f, 26.f, RS_SCREEN_W, 88.f, wash,
                         rsWithAlpha(wash, 0));

        /* Barely-visible RetroShell cross watermark, aligned to the 8px grid. */
        const u32 mark =
            rsWithAlpha(m_pal.accent, m_pal.dark ? 8u : 5u);
        m_renderer.rect(428.f, 204.f, 8.f, 8.f, mark);
        m_renderer.rect(412.f, 204.f, 8.f, 8.f, mark);
        m_renderer.rect(444.f, 204.f, 8.f, 8.f, mark);
        m_renderer.rect(428.f, 188.f, 8.f, 8.f, mark);
        m_renderer.rect(428.f, 220.f, 8.f, 8.f, mark);
    }
}

void App::drawTopBar() {
    const auto& f = m_fonts;
    f.small.draw(m_renderer, 12.f, 1.f, "RetroShell", m_pal.textDim);

    /* Clock, right-aligned, with a pixel-snapped battery to its right. */
    int hh = 0, mm = 0;
    power::clockNow(hh, mm);
    char clock[12];
    if (cfg::get().clock24Hour) {
        std::snprintf(clock, sizeof clock, "%02d:%02d", hh, mm);
    } else {
        const char* suffix = hh < 12 ? "AM" : "PM";
        const int displayHour = (hh % 12) == 0 ? 12 : hh % 12;
        std::snprintf(clock, sizeof clock, "%d:%02d %s",
                      displayHour, mm, suffix);
    }
    f.small.draw(m_renderer, RS_SCREEN_W - 40.f, 1.f, clock,
                 m_pal.textSecondary,
                 text::Align::Right);
    ui::prim::battery(m_renderer, RS_SCREEN_W - 32.f, 3.f,
                      m_batteryPct < 0 ? -1.f : float(m_batteryPct) / 100.f,
                      m_batteryChg, m_pal.textSecondary, m_pal.accent);
    /* One shared chrome rail for every frontend scene. */
    m_renderer.rect(
        0.f, 15.f, RS_SCREEN_W, 1.f,
        rsWithAlpha(m_pal.panelOutline,
                    rsClamp<u32>(
                        u32(rsAlphaOf(m_pal.panelOutline) * 2u), 28u, 76u)));
}

void App::drawHintBar(const Hint* hints, int count) {
    m_renderer.rect(
        0.f, RS_SCREEN_H - 26.f, RS_SCREEN_W, 1.f,
        rsWithAlpha(m_pal.panelOutline,
                    rsClamp<u32>(
                        u32(rsAlphaOf(m_pal.panelOutline) * 2u), 28u, 76u)));
    m_fonts.small.draw(m_renderer, 20.f, RS_SCREEN_H - 17.f, "Actions",
                       m_pal.textDim);

    float x = RS_SCREEN_W - 20.f;
    for (int i = count - 1; i >= 0; i--) {
        const float w = m_fonts.small.measure(hints[i].label);
        x -= w;
        m_fonts.small.draw(m_renderer, x, RS_SCREEN_H - 17.f, hints[i].label,
                           m_pal.textSecondary);
        x -= 16.f;
        ui::prim::buttonGlyph(m_renderer, hints[i].button, x,
                              RS_SCREEN_H - 10.f, 6.f, m_pal.textSecondary);
        x -= 16.f;
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

    const float w =
        rsClamp(m_fonts.body.measure(m_toastMsg) + 32.f, 64.f, 448.f);
    const float x = (RS_SCREEN_W - w) / 2.f;
    const float y = RS_SCREEN_H - 72.f;
    ui::prim::roundedRect(m_renderer, x, y, w, 32.f, 12.f,
                          rsWithAlpha(darkTheme() ? rsHex(0x2A3242)
                                                  : rsHex(0x353C4A),
                                      alpha));
    m_renderer.setScissor(int(x + 16.f), int(y), int(w - 32.f), 32);
    m_fonts.body.draw(m_renderer, RS_SCREEN_W / 2.f, y + 8.f, m_toastMsg,
                      rsWithAlpha(rsHex(0xF2F5FA), alpha),
                      text::Align::Center);
    m_renderer.resetScissor();
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
    /* Keep diagnostics in the otherwise-empty centre of the top bar. The
     * old y=24 placement obscured every scene title and made correct layout
     * look broken whenever Show FPS was enabled. */
    ui::prim::roundedRect(m_renderer, 176.f, 4.f, 128.f, 18.f, 6.f,
                          rsHex(0x000000, 140));
    m_fonts.small.draw(m_renderer, 240.f, 7.f, buf, rsHex(0x7CFF9B),
                       text::Align::Center);
}
#endif

}  // namespace rs
