#include "frontend/app.h"
#include "frontend/core_package_installer.h"
#include "frontend/autopilot.h"
#include "frontend/game_session.h"
#include "frontend/native_emulator_launcher.h"
#include "frontend/launch_notice.h"
#include "frontend/scenes/boot_scene.h"
#include "frontend/ui/chrome.h"
#include "platform/psp/audio_out.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/power.h"
#include "platform/psp/vram.h"
#include "core_api/rs_core_api.h"

#include "rs_build_stamp.h"
#include "runtime/arena.h"
#include "runtime/config.h"
#include "runtime/log.h"

#include <pspkernel.h>
#include <pspgu.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "stb_image.h"

/* Baked font atlases embedded at build time (see cmake/rs_assets.cmake). */
#include "rs_asset_font_display_rsf.h"
#include "rs_asset_font_title_rsf.h"
#include "rs_asset_font_body_strong_rsf.h"
#include "rs_asset_font_body_rsf.h"
#include "rs_asset_font_small_rsf.h"
#include "rs_asset_font_tiny_rsf.h"
#include "rs_asset_splash_png.h"

/* Set by the HOME-menu exit callback in main.cpp. */
extern volatile bool g_exitRequested;

namespace rs {

namespace {

void drawStartupSubtitle(gfx::Renderer& renderer, const text::Font& font) {
    constexpr const char* value = "PSP Retro Emulation";
    constexpr float tracking = .2f;
    const int count = int(std::strlen(value));
    float width = count > 1 ? tracking * float(count - 1) : 0.f;
    for (int i = 0; i < count; ++i) {
        const char glyph[2] = {value[i], '\0'};
        width += font.measure(glyph);
    }
    float x = RS_SCREEN_W * .5f - width * .5f;
    for (int i = 0; i < count; ++i) {
        const char glyph[2] = {value[i], '\0'};
        font.draw(renderer, x, 181.f, glyph, rsHex(0xD79A2B));
        x += font.measure(glyph) + tracking;
    }
}

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
    if (ready) {
        renderer.sprite(splash, 0, 0, w, h, 0, 0,
                        RS_SCREEN_W, RS_SCREEN_H, rsHex(0xFFFFFF));
        /* Replace the authored left-to-right subtitle gradient with the same
         * solid orange base used by BootScene. Once initialization completes,
         * BootScene adds the animated shimmer without a visible color jump. */
        text::Font startupFont;
        if (startupFont.load(rs_asset_font_small_rsf,
                             rs_asset_font_small_rsf_len)) {
            renderer.rect(0.f, 176.f, RS_SCREEN_W, 22.f, rsHex(0xFAF5EE));
            drawStartupSubtitle(renderer, startupFont);
            renderer.endFrame();
            startupFont.unload();
        } else {
            renderer.endFrame();
        }
    } else {
        renderer.endFrame();
    }

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
    nativeemu::consumeReturnReceipt();
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
        {&m_fonts.display, rs_asset_font_display_rsf,
                           rs_asset_font_display_rsf_len},
        {&m_fonts.title, rs_asset_font_title_rsf, rs_asset_font_title_rsf_len},
        {&m_fonts.bodyStrong, rs_asset_font_body_strong_rsf,
                              rs_asset_font_body_strong_rsf_len},
        {&m_fonts.body,  rs_asset_font_body_rsf,  rs_asset_font_body_rsf_len},
        {&m_fonts.small, rs_asset_font_small_rsf, rs_asset_font_small_rsf_len},
        {&m_fonts.tiny, rs_asset_font_tiny_rsf, rs_asset_font_tiny_rsf_len},
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
    RS_LOGI("build: RetroShell %s (%s UTC), core API v%u",
            RS_RELEASE_VERSION, RS_BUILD_STAMP, unsigned(RS_CORE_API_VERSION));
    RS_LOGI("hardware: %u KB core arena",
            unsigned(mem::totalSize() / 1024));
    power::setCpuMhz(cfg::get().cpuMenuMhz);

    const corepkg::InstallReport packageReport =
        corepkg::installDroppedPackages();

    /* Fonts and primitive masks stay resident across core launches;
     * everything allocated after this mark is evictable. */
    gfx::vram::setBootMark();

    m_theme = theme::loadTheme(cfg::get().theme);
    m_pal = theme::personalize(m_theme.palette, cfg::get().accent);
    m_themeFrom = m_pal;

    m_library.load();
    /* Resume exactly where the user left the shell, even across a process
     * replacement (native emulators) or a power cycle. */
    if (const auto& loc = m_library.location(); loc.valid) {
        m_snapshot.systemId = loc.system;
        m_snapshot.layer = nav::Layer(rsClamp(loc.layer, 0, 3));
        m_snapshot.continueIdx = loc.continueIdx;
    }
    const bool libraryCached = m_index.loadCache();
    if (!libraryCached) RS_LOGI("index: no cache yet");
    m_cores.discover();
    if (!libraryCached || m_index.totalCount() == 0) {
        m_scanner.start();
    } else {
        RS_LOGI("scanner: using cached library; manual rescan available");
        m_thumbs.rebuild(m_index, false);
    }

    m_lastUs = sceKernelGetSystemTimeLow();
    switchScene(std::make_unique<BootScene>(), /*instant=*/true);
    if (packageReport.installed || packageReport.failed) {
        char msg[96];
        if (packageReport.failed)
            std::snprintf(msg, sizeof msg, "Cores installed: %d  Failed: %d",
                          packageReport.installed, packageReport.failed);
        else
            std::snprintf(msg, sizeof msg, "%d core%s installed",
                          packageReport.installed,
                          packageReport.installed == 1 ? "" : "s");
        toast(msg);
    }
    RS_LOGI("app: init complete in %u ms",
            unsigned((sceKernelGetSystemTimeLow() - initStart) / 1000));
    mem::logHeapUsage("after boot");
    return true;
}

bool App::takeLaunchError(char* out, size_t size) {
    if (!m_launchError[0]) return false;
    std::snprintf(out, size, "%s", m_launchError);
    m_launchError[0] = '\0';
    return true;
}

void App::launchGame(const db::GameEntry& game, const CoreInfo* core) {
    m_launchError[0] = '\0';
    auto fail = [&](const char* reason) {
        std::snprintf(m_launchError, sizeof m_launchError, "%s", reason);
        RS_LOGW("app: launch refused for '%s': %s", game.name.c_str(), reason);
    };
    if (!core) core = m_cores.resolve(game);
    if (!core) {
        fail("core not installed");
        return;
    }
    /* Preflight while the frontend is still intact: a missing ROM found after
     * the bi-layer eviction would cost a full teardown and rebuild just to
     * print an error. One stat is cheaper than a failed launch. */
    if (!fs::exists(game.path.c_str())) {
        fail(fs::exists(fs::ROOT) ? "rom missing" : "storage unavailable");
        return;
    }
    RS_LOGI("app: launching '%s' via %s", game.name.c_str(),
            core->name.c_str());
    if (core->isNative()) {
        /* Commit launcher-owned state before replacing the process. Native
         * emulators own their saves, renderer, audio, timing and suspend
         * lifecycle; RetroShell intentionally does not stay resident. */
        m_scanner.stop();
        m_library.notePlayed(game.pathHash, power::localTimestamp());
        m_library.save();
        cfg::setGameOption(game.pathHash, "core", core->name.c_str());
        audio::setPaused(true);
        const int result = nativeemu::launch(*core, game);
        /* Only reached when the process replacement did NOT happen, so this
         * is always a failure. A raw status code is useless to someone who
         * simply has not supplied a BIOS yet — name the actual cause. */
        audio::setPaused(false);
        const char* reason;
        switch (result) {
            case -2: reason = "emulator is not installed"; break;
            case -5: reason = "ROM path is too long"; break;
            case -8: reason = "could not create its working folders"; break;
            case -7: reason = core->biosSource.empty()
                                  ? "required BIOS is missing"
                                  : "needs a valid BIOS in RETROSHELL/system";
                     break;
            default: reason = "emulator could not be started"; break;
        }
        char msg[96];
        std::snprintf(msg, sizeof msg, "%s: %s", core->name.c_str(), reason);
        fail(msg);
        RS_LOGE("app: native launch failed (%d): %s", result, msg);
        return;
    }
    switchScene(std::make_unique<GameSession>(game, core->name));
}

void App::evictForCore() {
    /* A scanner competes for the 4 MB newlib heap and Memory Stick while a
     * core is loading. Stop and join it before changing the memory map. */
    m_scanner.stop();
    m_thumbs.suspend();
    m_boxart.clear();
    m_theme.freeAssets();
    gfx::vram::freeToBootMark();
    RS_LOGI("app: evicted frontend caches (%u KB arena, %u KB vram free)",
            unsigned(mem::available() / 1024),
            unsigned(gfx::vram::available() / 1024));
    /* The decisive sample: whatever the newlib heap still holds here is held
     * for the whole core session. The gap between this and the fixed
     * PSP_HEAP_SIZE_KB reservation is what could be returned to the arena. */
    mem::logHeapUsage("core launch");
}

void App::restoreAfterCore() {
    m_theme = theme::loadTheme(m_theme.id);
    m_pal = theme::personalize(m_theme.palette, cfg::get().accent);
    m_themeFrom = m_pal;
    /* Finish any thumbnail work the launch interrupted; already-built
     * systems cost one read each, on the worker. */
    m_thumbs.rebuild(m_index, false);
    RS_LOGI("app: frontend restored");
}

void App::shutdown() {
    m_scanner.stop();
    m_thumbs.suspend();
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
    for (text::Font* f : {&m_fonts.display, &m_fonts.title, &m_fonts.bodyStrong,
                          &m_fonts.body, &m_fonts.small, &m_fonts.tiny})
        f->unload();
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
        const u32 powerEvents = power::consumeEvents();
        if (powerEvents & power::EVENT_SUSPENDING) {
            audio::setPaused(true);
            if (m_scene) m_scene->systemSuspend(*this);
            RS_LOGI("power: suspend acknowledged");
        }
        if (powerEvents & power::EVENT_RESUMED) {
            /* The PSP may restore its default clocks and controller history
             * after sleep. Re-establish frontend-owned state before the next
             * update or emulated frame. */
            audio::setPaused(true);
            audio::clear();
            m_pad.init();
            m_pad.resetAfterResume();
            m_lastUs = sceKernelGetSystemTimeLow();
            power::setCpuMhz(cfg::get().cpuMenuMhz);
            if (m_scene) m_scene->systemResume(*this);
            RS_LOGI("power: resume complete");
        }
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
                           PSP_CTRL_LEFT | PSP_CTRL_RIGHT |
                           PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER)) {
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
        m_thumbs.rebuild(m_index, false);
        RS_LOGI("index: refreshed, %d games", m_index.totalCount());
    }
    m_thumbs.update();

    /* Battery/clock polling is cheap but not free — every ~2s. */
    if (--m_batteryPoll <= 0) {
        m_batteryPoll = 120;
        m_batteryPct = power::batteryPercent();
        m_batteryChg = power::batteryCharging();
        /* Announce each low-battery threshold once per discharge. */
        if (m_batteryChg || m_batteryPct < 0) {
            m_batteryWarned = 100;
        } else if (m_batteryPct <= 5 && m_batteryWarned > 5) {
            m_batteryWarned = 5;
            toast("Battery critical - connect charger");
        } else if (m_batteryPct <= 15 && m_batteryWarned > 15) {
            m_batteryWarned = 15;
            toast("Low battery");
        }
    }

    m_toastTween.update(dt);

#ifdef RS_DEBUG_OVERLAY
    if (m_pad.isPressed(PSP_CTRL_TRIANGLE) && m_pad.isHeld(PSP_CTRL_LTRIGGER))
        m_showOverlay = !m_showOverlay;
#endif

    if (m_scene && !m_fadingOut) m_scene->update(*this, dt);
}

void App::draw() {
    m_renderer.beginFrame(m_pal.bg);
    if (m_scene) m_scene->draw(*this);

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
        /* Built-in themes are one uninterrupted field. Gradients, ambient
         * washes and watermarks created visible blocks on original LCD and
         * replacement IPS panels. */
        m_renderer.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H, m_pal.bg);
    }
    if (m_theme.waves) {
        /* Explicit custom-theme compatibility. Built-in themes use the
         * quieter static treatment below. */
        drawWave(158.f, 16.f, 1.0f, 0.45f, 0.0f, 130.f, m_pal.waveA);
        drawWave(186.f, 12.f, 1.4f, 0.32f, 2.1f, 100.f, m_pal.waveB);
    }
}

/* One header for every screen: mark + RETROSHELL on the left, clock and
 * battery on the right. Fixed positions and a fixed baseline — nothing here
 * moves between screens. */
void App::drawTopBar(u32 alpha) {
    using ui::fade;
    namespace L = ui::layout;
    const auto& f = m_fonts;
    constexpr float BAND = L::HEADER_RULE_Y;           /* 0..26 */
    const float statusY = f.small.centerY(0.f, BAND);

    ui::brandMark(m_renderer, L::MARGIN, float(int((BAND - 10.f) * .5f)), 2,
                  fade(m_pal.textSecondary, alpha));
    f.title.draw(m_renderer, L::MARGIN + 18.f, f.title.centerY(0.f, BAND),
                 "RETROSHELL", fade(m_pal.textPrimary, alpha),
                 text::Align::Left, 2.f);

    /* Status cluster, right to left: battery, percentage, clock. */
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
    constexpr float BATTERY_W = 20.f;                  /* body + nub */
    const float batteryX = L::RIGHT - BATTERY_W;
    /* Low battery (not charging) turns the readout red; below 5% it blinks. */
    const bool low = m_batteryPct >= 0 && !m_batteryChg && m_batteryPct <= 15;
    const bool blinkOff = low && m_batteryPct <= 5 &&
                          std::fmod(m_time, 1.f) > .6f;
    const u32 ink = fade(low ? m_pal.danger : m_pal.textPrimary, alpha);
    if (!blinkOff)
        ui::prim::battery(m_renderer, batteryX, float(int((BAND - 8.f) * .5f)),
                          m_batteryPct < 0 ? -1.f
                                           : float(m_batteryPct) / 100.f,
                          m_batteryChg, ink,
                          fade(low ? m_pal.danger : m_pal.focusEdge, alpha));
    float right = batteryX - 8.f;
    if (m_batteryPct >= 0) {
        char pct[16];
        std::snprintf(pct, sizeof pct, "%d%%", m_batteryPct);
        f.small.draw(m_renderer, right, statusY, pct, ink, text::Align::Right);
        right -= f.small.measure(pct) + 14.f;
    }
    f.small.draw(m_renderer, right, statusY, clock,
                 fade(m_pal.textPrimary, alpha), text::Align::Right);
    right -= f.small.measure(clock) + 16.f;

    /* A background scan announces itself in the same quiet cluster. */
    if (m_scanner.running()) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "Scanning %d", m_scanner.progress());
        f.tiny.draw(m_renderer, right, f.tiny.centerY(0.f, BAND), buf,
                    fade(m_pal.textMuted, alpha), text::Align::Right);
        const float dotX = right - f.tiny.measure(buf) - 8.f;
        const int phase = int(m_time * 3.f) % 3;
        for (int i = 0; i < 3; i++)
            m_renderer.rect(dotX - float(2 - i) * 4.f, float(int(BAND * .5f)),
                            2.f, 2.f,
                            fade(i == phase ? m_pal.focusEdge : m_pal.textMuted,
                                 alpha));
    }
}

/* One line, spread across the width in equal columns (at least three), so
 * each action keeps its place from screen to screen. */
void App::drawHintBar(const Hint* hints, int count, bool solid) {
    namespace L = ui::layout;
    constexpr float GLYPH_GAP = 7.f;    /* glyph to label */
    const float bandTop = L::FOOTER_RULE_Y + 1.f;
    const float bandH = RS_SCREEN_H - bandTop;
    const float cy = float(int(bandTop + bandH * .5f));
    const auto& font = m_fonts.small;
    const float textY = font.centerY(bandTop, bandH);

    if (solid)
        m_renderer.rect(0.f, L::FOOTER_RULE_Y, RS_SCREEN_W,
                        RS_SCREEN_H - L::FOOTER_RULE_Y,
                        rsWithAlpha(m_pal.bg, 236));
    m_renderer.rect(L::MARGIN, L::FOOTER_RULE_Y, L::RIGHT - L::MARGIN, 1.f,
                    m_pal.line);

    if (count > 8) count = 8;
    /* A glyph with an empty label pairs with the next one ("L1 R1 Tab"). */
    int groups = 0;
    for (int i = 0; i < count; i++)
        if (hints[i].label[0]) groups++;
    /* At least three columns, so a short legend still reads left to right
     * rather than stretching to the far edge. */
    const int columns = groups > 3 ? groups : 3;
    const float colW = (L::RIGHT - L::MARGIN) / float(columns);
    float x = L::MARGIN + 4.f;
    int group = 0;
    for (int i = 0; i < count; i++) {
        const float lead = ui::prim::buttonGlyphWidth(hints[i].button);
        ui::prim::buttonGlyph(m_renderer, hints[i].button, x + lead * .5f, cy,
                              6.f, m_pal.textPrimary);
        if (!hints[i].label[0]) {
            x += lead + 4.f;
            continue;
        }
        font.draw(m_renderer, x + lead + GLYPH_GAP, textY, hints[i].label,
                  m_pal.textSecondary);
        group++;
        x = L::MARGIN + 4.f + colW * float(group);
    }
}

void App::drawToast() {
    if (!m_toastTween.running() || !m_toastMsg[0]) return;
    const float t = m_toastTween.t;
    /* Quick fade in, hold, fade out. */
    float a = 1.f;
    if (t < 0.06f) a = t / 0.06f;
    else if (t > 0.85f) a = (1.f - t) / 0.15f;
    const u32 alpha = u32(a * 255.f);

    /* A compact pill parked above the legend, right-aligned to the margin,
     * so it never lands on list text or covers a focused row. */
    namespace L = ui::layout;
    const auto& font = m_fonts.small;
    constexpr float H = 20.f;
    const float w = rsClamp(float(int(font.measure(m_toastMsg))) + 24.f, 64.f,
                            L::RIGHT - L::MARGIN);
    const float x = L::RIGHT - w;
    const float y = L::FOOTER_RULE_Y - 6.f - H;
    ui::pixelRect(m_renderer, x, y, w, H, 3,
                  ui::fade(m_pal.surface2, alpha));
    ui::pixelFrame(m_renderer, x, y, w, H, 1, 3, ui::fade(m_pal.line, alpha));
    m_renderer.rect(x + 9.f, y + 9.f, 2.f, 2.f,
                    ui::fade(m_pal.focusEdge, alpha));
    m_renderer.setScissor(int(x + 4.f), int(y), int(w - 8.f), int(H));
    font.draw(m_renderer, x + 16.f, font.centerY(y, H), m_toastMsg,
              ui::fade(m_pal.textPrimary, alpha));
    m_renderer.resetScissor();
}

#ifdef RS_DEBUG_OVERLAY
void App::drawDebugOverlay() {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.1f fps  %.2f ms", m_renderer.fps(),
                  m_renderer.frameMs());
    /* Keep diagnostics in the otherwise-empty centre of the top bar. The
     * old y=24 placement obscured every scene title and made correct layout
     * look broken whenever Show FPS was enabled. */
    ui::prim::roundedRect(m_renderer, 216.f, 4.f, 112.f, 18.f, 6.f,
                          rsHex(0x000000, 140));
    m_fonts.small.draw(m_renderer, 272.f, 7.f, buf, rsHex(0x7CFF9B),
                       text::Align::Center);
}
#endif

}  // namespace rs
