/** Application shell: main loop, scene stack, theme state, library
 * services, and the shared chrome (animated background, top bar, hint bar,
 * toasts) every scene draws through.
 */
#pragma once

#include "frontend/database/game_index.h"
#include "frontend/database/library.h"
#include "frontend/database/metadata.h"
#include "frontend/database/rom_scanner.h"
#include "frontend/scenes/scene.h"
#include "frontend/text/font.h"
#include "frontend/themes/theme.h"
#include "frontend/ui/anim.h"
#include "frontend/ui/prim.h"
#include "platform/psp/gu_renderer.h"
#include "platform/psp/input_pad.h"
#include "rs_common.h"

#include <memory>

namespace rs {

/* Lightweight UI state that survives scene switches and (Phase 3) the
 * frontend teardown around a core launch — the "FrontendSnapshot" of the
 * bi-layer protocol. Plain data only. */
struct FrontendSnapshot {
    int  catIdx  = 2;      /* home category */
    int  listIdx = 0;
    bool inList  = false;
};

class App {
public:
    bool init();
    void shutdown();
    void run();

    /* --- services ------------------------------------------------------ */
    gfx::Renderer&    renderer()       { return m_renderer; }
    const input::Pad& pad() const      { return m_pad; }
    input::Pad&       padMutable()     { return m_pad; }

    db::GameIndex&    index()          { return m_index; }
    db::Library&      library()        { return m_library; }
    db::BoxartCache&  boxart()         { return m_boxart; }
    db::RomScanner&   scanner()        { return m_scanner; }
    FrontendSnapshot& snapshot()       { return m_snapshot; }

    struct Fonts {
        text::Font title;   /* Inter SemiBold 26 */
        text::Font large;   /* Inter SemiBold 19 */
        text::Font body;    /* Inter Regular 15  */
        text::Font small;   /* Inter Regular 12  */
    };
    const Fonts& fonts() const { return m_fonts; }

    /* --- theme ---------------------------------------------------------- */
    const theme::Palette& pal() const { return m_pal; }
    const theme::Theme& theme() const { return m_theme; }
    bool darkTheme() const            { return m_theme.palette.dark; }
    void setThemeById(const std::string& id);   /* animated crossfade */
    void toggleTheme();

    /* --- scenes ---------------------------------------------------------- */
    /* Takes ownership; transitions through a brief scrim fade. */
    void switchScene(std::unique_ptr<Scene> next, bool instant = false);

    /* --- shared chrome ---------------------------------------------------- */
    void drawBackground();
    void drawTopBar();
    struct Hint { ui::prim::Button button; const char* label; };
    void drawHintBar(const Hint* hints, int count);
    void toast(const char* msg);

    float time() const { return m_time; }

private:
    void update(float dt);
    void draw();
    void drawWave(float baseY, float amp, float freq, float speed,
                  float phase, float height, u32 color);
    void drawToast();
    void drawScanStatus();
#ifdef RS_DEBUG_OVERLAY
    void drawDebugOverlay();
#endif

    gfx::Renderer m_renderer;
    input::Pad    m_pad;
    Fonts         m_fonts;

    db::GameIndex   m_index;
    db::RomScanner  m_scanner;
    db::Library     m_library;
    db::BoxartCache m_boxart;
    FrontendSnapshot m_snapshot;

    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<Scene> m_pending;
    ui::Tween m_sceneFade;          /* 0..1: out then in */
    bool m_fadingOut = false;

    theme::Theme   m_theme;
    theme::Palette m_pal;
    theme::Palette m_themeFrom;
    ui::Tween m_themeFade;

    char m_toastMsg[96] = {};
    ui::Tween m_toastTween;

    float m_time = 0.f;
    u32   m_lastUs = 0;

    int  m_batteryPct   = -1;
    bool m_batteryChg   = false;
    int  m_batteryPoll  = 0;

#ifdef RS_DEBUG_OVERLAY
    bool m_showOverlay = false;
#endif
};

}  // namespace rs
