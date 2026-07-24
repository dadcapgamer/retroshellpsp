/** GameSession — the scene that runs an emulator core.
 *
 * Owns the bi-layer launch protocol:
 *   enter:  evict frontend caches → (PRX mode: release arena, load core
 *           module, re-reserve arena) → load ROM into the arena → restore
 *           SRAM → run
 *   exit:   flush SRAM → unload ROM & core → restore arena → reload theme
 *           assets → return to Home with the browser state intact
 *
 * The in-game menu (L+R+START) pauses emulation and offers save states
 * with thumbnails, reset, screenshot and exit.
 */
#pragma once

#include "frontend/core_manager.h"
#include "frontend/database/game_index.h"
#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"
#include "platform/psp/gu_renderer.h"
#include "platform/psp/input_pad.h"
#include "runtime/save_manager.h"

namespace rs {

class GameSession : public Scene {
public:
    explicit GameSession(db::GameEntry game) : m_game(std::move(game)) {}

    void enter(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    enum class State { Starting, Running, Menu, Failed, Exiting };
    enum MenuRow {
        MENU_RESUME = 0,
        MENU_SAVE,
        MENU_LOAD,
        MENU_RESET,
        MENU_SCREENSHOT,
        MENU_EXIT,
        MENU_COUNT
    };

    bool startCore(App& app);
    void exitToHome(App& app);
    void updateRunning(App& app, float dt);
    void updateMenu(App& app);
    void drawFrame(App& app);
    void drawMenu(App& app);
    void openMenu(App& app);
    void makeThumb(u16* out) const;
    u32  mapButtons(const input::Pad& pad) const;

    db::GameEntry m_game;
    CoreManager m_cores;
    State m_state = State::Starting;

    gfx::Texture m_frameTex;
    u16 m_frameW = 0, m_frameH = 0;

    int m_menuRow = 0;
    int m_slot = 0;
    ui::Smooth m_menuPos;
    ui::Tween m_menuFade;
    save::SlotInfo m_slots[save::SLOTS];
    gfx::Texture m_thumbTex;
    int m_thumbSlot = -1;          /* slot currently in m_thumbTex */

    float m_sramTimer = 0.f;
    char m_error[96] = {};
};

}  // namespace rs
