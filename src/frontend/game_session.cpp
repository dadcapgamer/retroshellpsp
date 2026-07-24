#include "frontend/game_session.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "platform/psp/audio_out.h"
#include "platform/psp/fs_psp.h"
#include "platform/psp/power.h"
#include "platform/psp/vram.h"
#include "runtime/arena.h"
#include "runtime/config.h"
#include "runtime/host_services.h"
#include "runtime/log.h"

#include "miniz.h"

#include <pspctrl.h>
#include <pspgu.h>

#include <cstdio>
#include <cstring>

namespace rs {

namespace {
constexpr u32 MENU_COMBO = PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER;
constexpr float SRAM_FLUSH_SECONDS = 10.f;

const char* MENU_LABELS[] = {
    "Resume", "Save state", "Load state", "Reset", "Screenshot", "Exit game",
};
}  // namespace

/* ---------------------------------------------------------------------- */
/* Launch / teardown                                                       */
/* ---------------------------------------------------------------------- */

bool GameSession::startCore(App& app) {
    /* 1. Evict frontend caches (the arena/vram space becomes the core's). */
    app.evictForCore();

#ifndef RS_STATIC_CORES
    /* 2. PRX mode: the module loader needs partition space, so the arena
     * is released around the load and re-reserved afterwards. */
    mem::shutdown();
#endif
    const char* coreName = CoreManager::coreForSystem(m_game.system);
    const bool loaded = m_cores.loadCore(coreName);
#ifndef RS_STATIC_CORES
    if (!mem::init()) {
        std::snprintf(m_error, sizeof m_error, "arena re-reserve failed");
        return false;
    }
#endif
    if (!loaded) {
        std::snprintf(m_error, sizeof m_error, "%s", m_cores.error());
        return false;
    }
    /* Init only now: the arena is guaranteed to be in place. */
    if (!m_cores.core().initialize(host::table())) {
        std::snprintf(m_error, sizeof m_error, "core init failed");
        return false;
    }

    /* 3. ROM into the arena. */
    u8* romData = nullptr;
    u32 romSize = 0;
    if (!m_game.zipEntry.empty()) {
        mz_zip_archive zip;
        std::memset(&zip, 0, sizeof zip);
        if (!mz_zip_reader_init_file(&zip, m_game.path.c_str(), 0)) {
            std::snprintf(m_error, sizeof m_error, "zip open failed");
            return false;
        }
        const int idx =
            mz_zip_reader_locate_file(&zip, m_game.zipEntry.c_str(), nullptr, 0);
        mz_zip_archive_file_stat st;
        if (idx < 0 || !mz_zip_reader_file_stat(&zip, mz_uint(idx), &st)) {
            mz_zip_reader_end(&zip);
            std::snprintf(m_error, sizeof m_error, "zip entry missing");
            return false;
        }
        romSize = u32(st.m_uncomp_size);
        romData = static_cast<u8*>(mem::alloc(romSize, 64));
        if (!romData ||
            !mz_zip_reader_extract_to_mem(&zip, mz_uint(idx), romData, romSize,
                                          0)) {
            mz_zip_reader_end(&zip);
            std::snprintf(m_error, sizeof m_error, "zip extract failed");
            return false;
        }
        mz_zip_reader_end(&zip);
    } else {
        const s32 size = fs::fileSize(m_game.path.c_str());
        if (size <= 0) {
            std::snprintf(m_error, sizeof m_error, "rom missing");
            return false;
        }
        romSize = u32(size);
        romData = static_cast<u8*>(mem::alloc(romSize, 64));
        if (!romData ||
            fs::readRange(m_game.path.c_str(), romData, 0, romSize) !=
                s32(romSize)) {
            std::snprintf(m_error, sizeof m_error, "rom read failed");
            return false;
        }
    }

    /* 4. Boot the core. */
    host::setActiveGame(m_game.pathHash);
    if (!m_cores.core().loadROM(m_game.path.c_str(), romData, romSize)) {
        std::snprintf(m_error, sizeof m_error, "core rejected rom");
        return false;
    }
    save::loadSram(m_game, m_cores.core());
    audio::clear();

    power::setCpuMhz(cfg::get().cpuGameMhz);
    app.library().notePlayed(m_game.pathHash);
    RS_LOGI("session: '%s' running (%u KB arena free)", m_game.name.c_str(),
            unsigned(mem::available() / 1024));
    return true;
}

void GameSession::enter(App& app) {
    if (startCore(app)) {
        m_state = State::Running;
        app.toast("L + R + START for menu");
    } else {
        RS_LOGE("session: launch failed: %s", m_error);
        m_state = State::Failed;
    }
}

void GameSession::exitToHome(App& app) {
    if (m_cores.loaded()) {
        save::saveSram(m_game, m_cores.core());
        m_cores.core().unloadROM();
    }
    gfx::Renderer::freeTexture(m_frameTex);
    gfx::Renderer::freeTexture(m_thumbTex);

#ifndef RS_STATIC_CORES
    mem::shutdown();
    m_cores.unloadCore();
    mem::init();
#else
    m_cores.unloadCore();
    mem::reset(0);
#endif
    audio::clear();
    host::setActiveGame(0);
    power::setCpuMhz(cfg::get().cpuMenuMhz);

    app.restoreAfterCore();
    m_state = State::Exiting;
    app.switchScene(std::make_unique<HomeScene>());
}

/* ---------------------------------------------------------------------- */
/* Input mapping                                                           */
/* ---------------------------------------------------------------------- */

u32 GameSession::mapButtons(const input::Pad& pad) const {
    /* Defaults; per-game remapping arrives with the settings overlay. */
    const u32 held = pad.held();
    u32 out = 0;
    if (held & PSP_CTRL_UP) out |= RS_BTN_UP;
    if (held & PSP_CTRL_DOWN) out |= RS_BTN_DOWN;
    if (held & PSP_CTRL_LEFT) out |= RS_BTN_LEFT;
    if (held & PSP_CTRL_RIGHT) out |= RS_BTN_RIGHT;
    if (held & PSP_CTRL_CIRCLE) out |= RS_BTN_A;
    if (held & PSP_CTRL_CROSS) out |= RS_BTN_B;
    if (held & PSP_CTRL_TRIANGLE) out |= RS_BTN_X;
    if (held & PSP_CTRL_SQUARE) out |= RS_BTN_Y;
    if (held & PSP_CTRL_LTRIGGER) out |= RS_BTN_L;
    if (held & PSP_CTRL_RTRIGGER) out |= RS_BTN_R;
    if (held & PSP_CTRL_START) out |= RS_BTN_START;
    if (held & PSP_CTRL_SELECT) out |= RS_BTN_SELECT;
    return out;
}

/* ---------------------------------------------------------------------- */
/* Update                                                                  */
/* ---------------------------------------------------------------------- */

void GameSession::updateRunning(App& app, float dt) {
    const auto& pad = app.pad();

    if ((pad.held() & MENU_COMBO) == MENU_COMBO &&
        pad.isPressed(PSP_CTRL_START)) {
        openMenu(app);
        return;
    }

    const u32 buttons = mapButtons(pad);
    host::setInputState(buttons);
    m_cores.core().runFrame(buttons);

    /* Periodic battery-save flush. */
    m_sramTimer += dt;
    if (m_sramTimer >= SRAM_FLUSH_SECONDS) {
        m_sramTimer = 0.f;
        if (cfg::get().autosave && m_cores.core().sramDirty())
            save::saveSram(m_game, m_cores.core());
    }
}

void GameSession::openMenu(App& app) {
    (void)app;
    m_state = State::Menu;
    m_menuRow = 0;
    m_menuPos.snap(0.f);
    m_menuFade.start(0.18f);
    save::querySlots(m_game, m_slots);
    m_thumbSlot = -1;
}

void GameSession::makeThumb(u16* out) const {
    /* Nearest-neighbour downsample of the core frame to thumbnail size. */
    const RSVideoFrame f = const_cast<CoreManager&>(m_cores).core().frame();
    if (!f.pixels || f.format != RS_PIXFMT_RGB565) {
        std::memset(out, 0, save::THUMB_W * save::THUMB_H * 2);
        return;
    }
    const u8* src = static_cast<const u8*>(f.pixels);
    for (int y = 0; y < save::THUMB_H; y++) {
        const int sy = y * f.height / save::THUMB_H;
        const u16* row = reinterpret_cast<const u16*>(src + sy * f.pitch);
        for (int x = 0; x < save::THUMB_W; x++)
            out[y * save::THUMB_W + x] = row[x * f.width / save::THUMB_W];
    }
}

void GameSession::updateMenu(App& app) {
    const auto& pad = app.pad();
    auto& core = m_cores.core();

    if (pad.navPressed(PSP_CTRL_UP) && m_menuRow > 0) m_menuRow--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_menuRow < MENU_COUNT - 1)
        m_menuRow++;

    if ((m_menuRow == MENU_SAVE || m_menuRow == MENU_LOAD)) {
        if (pad.navPressed(PSP_CTRL_LEFT) && m_slot > 0) m_slot--;
        if (pad.navPressed(PSP_CTRL_RIGHT) && m_slot < save::SLOTS - 1)
            m_slot++;
    }

    if (pad.isPressed(PSP_CTRL_CIRCLE)) {
        m_state = State::Running;
        return;
    }

    if (!pad.isPressed(PSP_CTRL_CROSS)) return;
    switch (m_menuRow) {
        case MENU_RESUME:
            m_state = State::Running;
            break;
        case MENU_SAVE: {
            u16 thumb[save::THUMB_W * save::THUMB_H];
            makeThumb(thumb);
            app.toast(save::saveState(m_game, core, m_slot, thumb)
                          ? "State saved"
                          : "Save failed");
            save::querySlots(m_game, m_slots);
            m_thumbSlot = -1;
            break;
        }
        case MENU_LOAD:
            if (m_slots[m_slot].exists) {
                app.toast(save::loadState(m_game, core, m_slot)
                              ? "State loaded"
                              : "Load failed");
                audio::clear();
                m_state = State::Running;
            } else {
                app.toast("Empty slot");
            }
            break;
        case MENU_RESET:
            core.reset();
            audio::clear();
            m_state = State::Running;
            app.toast("Reset");
            break;
        case MENU_SCREENSHOT: {
            char path[128];
            fs::mkdirs("ms0:/RETROSUITE/screenshots");
            std::snprintf(path, sizeof path,
                          "ms0:/RETROSUITE/screenshots/%08x_%u.png",
                          unsigned(m_game.pathHash),
                          unsigned(app.time() * 10.f));
            app.renderer().requestCapture(path);
            app.toast("Screenshot saved");
            m_state = State::Running;
            break;
        }
        case MENU_EXIT:
            exitToHome(app);
            break;
    }
}

void GameSession::update(App& app, float dt) {
    switch (m_state) {
        case State::Running:
            updateRunning(app, dt);
            break;
        case State::Menu:
            m_menuPos.to(float(m_menuRow));
            m_menuPos.update(dt, 14.f);
            m_menuFade.update(dt);
            updateMenu(app);
            break;
        case State::Failed:
            if (app.pad().isPressed(PSP_CTRL_CIRCLE) ||
                app.pad().isPressed(PSP_CTRL_CROSS))
                exitToHome(app);
            break;
        default:
            break;
    }
}

/* ---------------------------------------------------------------------- */
/* Draw                                                                    */
/* ---------------------------------------------------------------------- */

void GameSession::drawFrame(App& app) {
    auto& r = app.renderer();
    const RSVideoFrame f = m_cores.core().frame();
    if (!f.pixels || !f.width || !f.height) return;

    if (!m_frameTex.valid() || m_frameW != f.width || m_frameH != f.height) {
        gfx::Renderer::freeTexture(m_frameTex);
        const int psm = f.format == RS_PIXFMT_RGBA8888 ? GU_PSM_8888
                                                       : GU_PSM_5650;
        if (!gfx::Renderer::createTexture(m_frameTex, f.width, f.height, psm,
                                          nullptr, /*dynamic=*/true))
            return;
        m_frameW = f.width;
        m_frameH = f.height;
    }
    gfx::Renderer::updateTexture(m_frameTex, f.pixels, f.pitch);

    /* Scale mode: fit (default), stretch, or 1:1 via per-game option. */
    float dw, dh;
    const std::string mode = cfg::gameOption(m_game.pathHash, "scale");
    if (mode == "1:1") {
        dw = f.width;
        dh = f.height;
    } else if (mode == "stretch") {
        dw = RS_SCREEN_W;
        dh = RS_SCREEN_H;
    } else {
        const float s = rsClamp(float(RS_SCREEN_W) / f.width,
                                0.f, float(RS_SCREEN_H) / f.height);
        dw = f.width * s;
        dh = f.height * s;
    }
    r.setTexFilter(cfg::gameOption(m_game.pathHash, "filter") == "nearest"
                       ? gfx::TexFilter::Nearest
                       : gfx::TexFilter::Linear);
    r.sprite(m_frameTex, 0, 0, f.width, f.height, (RS_SCREEN_W - dw) / 2.f,
             (RS_SCREEN_H - dh) / 2.f, dw, dh, rsHex(0xFFFFFF));
    r.setTexFilter(gfx::TexFilter::Linear);
}

void GameSession::drawMenu(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const float k = ui::easeOutCubic(m_menuFade.t);
    const u32 a = u32(k * 255.f);

    r.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H, rsHex(0x06080C, u32(k * 170.f)));

    const float px = 40.f, pw = 220.f;
    const float py = 42.f, rowH = 27.f;
    const float ph = rowH * MENU_COUNT + 24.f;
    ui::prim::roundedRect(r, px, py, pw, ph, 12.f, rsHex(0x141A24, a));
    ui::prim::roundedOutline(r, px, py, pw, ph, 12.f,
                             rsWithAlpha(pal.accent, a / 3u));

    fonts.small.draw(r, px + 16.f, py + 8.f, m_game.name.c_str(),
                     rsWithAlpha(rsHex(0x8A93A6), a));

    const float listY = py + 26.f;
    const float hy = listY + m_menuPos.v * rowH;
    ui::prim::roundedRect(r, px + 8.f, hy, pw - 16.f, rowH - 3.f, 8.f,
                          rsHex(0xFFFFFF, a / 9u));
    r.rect(px + 12.f, hy + 5.f, 3.f, rowH - 13.f, rsWithAlpha(pal.accent, a));

    for (int i = 0; i < MENU_COUNT; i++) {
        const bool sel = i == m_menuRow;
        char label[48];
        if (i == MENU_SAVE || i == MENU_LOAD) {
            std::snprintf(label, sizeof label, "%s  < %d%s >", MENU_LABELS[i],
                          m_slot + 1, m_slots[m_slot].exists ? "" : " ·");
        } else {
            std::snprintf(label, sizeof label, "%s", MENU_LABELS[i]);
        }
        fonts.body.draw(r, px + 24.f, listY + float(i) * rowH + 4.f, label,
                        rsHex(sel ? 0xF2F5FA : 0xA9B3C4, a));
    }

    /* Slot thumbnail preview beside the panel. */
    if ((m_menuRow == MENU_SAVE || m_menuRow == MENU_LOAD) &&
        m_slots[m_slot].exists) {
        if (m_thumbSlot != m_slot) {
            static u16 thumb[save::THUMB_W * save::THUMB_H];
            if (save::loadThumb(m_game, m_slot, thumb)) {
                if (!m_thumbTex.valid())
                    gfx::Renderer::createTexture(m_thumbTex, save::THUMB_W,
                                                 save::THUMB_H, GU_PSM_5650,
                                                 nullptr, /*dynamic=*/true);
                gfx::Renderer::updateTexture(m_thumbTex, thumb,
                                             save::THUMB_W * 2);
                m_thumbSlot = m_slot;
            }
        }
        if (m_thumbSlot == m_slot && m_thumbTex.valid()) {
            const float tx = px + pw + 18.f, ty = py + 30.f;
            ui::prim::roundedRect(r, tx - 4.f, ty - 4.f,
                                  save::THUMB_W + 8.f, save::THUMB_H + 8.f,
                                  8.f, rsHex(0x141A24, a));
            r.sprite(m_thumbTex, 0, 0, save::THUMB_W, save::THUMB_H, tx, ty,
                     save::THUMB_W, save::THUMB_H, rsHex(0xFFFFFF, a));
            char cap[24];
            std::snprintf(cap, sizeof cap, "Slot %d", m_slot + 1);
            fonts.small.draw(r, tx + save::THUMB_W / 2.f,
                             ty + save::THUMB_H + 8.f, cap,
                             rsHex(0x8A93A6, a), text::Align::Center);
        }
    }

    const App::Hint hints[] = {
        {ui::prim::Button::Cross, "Select"},
        {ui::prim::Button::Circle, "Resume"},
    };
    app.drawHintBar(hints, 2);
}

void GameSession::draw(App& app) {
    auto& r = app.renderer();

    if (m_state == State::Failed) {
        app.drawBackground();
        const auto& pal = app.pal();
        app.fonts().large.draw(r, RS_SCREEN_W / 2.f, 100.f, "Launch failed",
                               pal.textPrimary, text::Align::Center);
        app.fonts().body.draw(r, RS_SCREEN_W / 2.f, 130.f, m_error,
                              pal.textSecondary, text::Align::Center);
        app.fonts().small.draw(r, RS_SCREEN_W / 2.f, 156.f,
                               "Press × to return", pal.textDim,
                               text::Align::Center);
        return;
    }
    if (m_state == State::Exiting || !m_cores.loaded()) return;

    /* Letterbox background stays pure black for the game frame. */
    r.rect(0, 0, RS_SCREEN_W, RS_SCREEN_H, rsHex(0x000000));
    drawFrame(app);
    if (m_state == State::Menu) drawMenu(app);
}

}  // namespace rs
