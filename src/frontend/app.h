/** Application shell: main loop, scene stack, theme state, library
 * services, and the shared chrome (animated background, top bar, hint bar,
 * toasts) every scene draws through.
 */
#pragma once

#include "frontend/core_registry.h"
#include "frontend/database/game_index.h"
#include "frontend/database/library.h"
#include "frontend/database/metadata.h"
#include "frontend/database/rom_scanner.h"
#include "frontend/database/thumb_cache.h"
#include "frontend/scenes/home_nav.h"
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
    nav::Layer layer = nav::Layer::Systems;
    int  systemId    = 0;   /* db::System of the rail selection (GB) */
    int  continueIdx = 0;
    /* Per-system game selection, the Library filter/sort and the rail
     * position are persisted in library.json rather than here, so they also
     * survive a process restart (native emulators replace the process). */
    /* Set by a failed launch's "Choose Emulator" action: Home opens the core
     * picker for this game as soon as it appears. */
    u32  pickerHash  = 0;
    /* Set when a failed launch should kick off a library rescan on return. */
    bool rescanOnHome = false;
    /* Set by Game Details' Save States: the launched game loads this slot
     * as soon as its core is up. -1 = start normally. */
    int  loadStateSlot = -1;
    /* Emulator Settings "Apply & Restart": Home relaunches this game with
     * this core as soon as it is up (loadStateSlot then carries it back to
     * where it was). 0 = nothing pending. */
    u32  relaunchHash = 0;
    char relaunchCore[32] = {};
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
    db::ThumbCache&   thumbs()         { return m_thumbs; }
    db::RomScanner&   scanner()        { return m_scanner; }
    CoreRegistry&     cores()          { return m_cores; }
    FrontendSnapshot& snapshot()       { return m_snapshot; }

    /* One family, IBM Plex Mono: SemiBold for emphasis, Regular for text.
     * Uppercase labels are drawn tracked (see Font::draw). */
    struct Fonts {
        text::Font display;     /* SemiBold 18 — system name, Detail title */
        text::Font title;       /* SemiBold 15 — wordmark, pane titles */
        text::Font bodyStrong;  /* SemiBold 13 — focused rows, card titles */
        text::Font body;        /* Regular 13  — rows, actions, labels */
        text::Font small;       /* Regular 11  — metadata, status, counts */
        text::Font tiny;        /* Regular 10  — chips, section labels, legend */
    };
    const Fonts& fonts() const { return m_fonts; }

    /* --- theme ---------------------------------------------------------- */
    const theme::Palette& pal() const { return m_pal; }
    const theme::Theme& theme() const { return m_theme; }
    bool darkTheme() const            { return m_theme.palette.dark; }
    void setThemeById(const std::string& id);   /* animated crossfade */
    void setAccentIndex(int index);

    /* --- scenes ---------------------------------------------------------- */
    /* Takes ownership; transitions through a brief scrim fade. */
    void switchScene(std::unique_ptr<Scene> next, bool instant = false);

    /* Starts a GameSession. `core` is the picker's explicit choice; by
     * default the game's core comes from CoreRegistry::resolve. Callers
     * that want the adaptive core step check cores().needsChoice() first
     * and open the picker instead. Toasts and stays put when no core
     * serves the game. */
    void launchGame(const db::GameEntry& game, const CoreInfo* core = nullptr);

    /* A launch that fails before any scene change (missing ROM, missing
     * emulator, native-launch refusal) leaves its raw reason here instead of
     * a toast, so the shell can present it as a proper error state. Returns
     * false when there is nothing pending. */
    bool takeLaunchError(char* out, size_t size);

    /* Bi-layer launch protocol (called by GameSession):
     * evictForCore drops every large frontend resource — box art, theme
     * assets, non-boot VRAM — so the core owns the memory; restoreAfterCore
     * rebuilds them when the game exits. */
    void evictForCore();
    void restoreAfterCore();

    /* --- shared chrome ---------------------------------------------------- */
    void drawBackground();
    /* The one header: mark + RETROSHELL left, clock and battery right. */
    void drawTopBar(u32 alpha = 255);
    struct Hint { ui::prim::Button button; const char* label; };
    /* The one footer: hairline, then glyph+label groups in equal columns
     * across the width. `solid` lays a band under it (over gameplay). */
    void drawHintBar(const Hint* hints, int count, bool solid = false);
    void toast(const char* msg);

    float time() const { return m_time; }

private:
    void update(float dt);
    void draw();
    void drawWave(float baseY, float amp, float freq, float speed,
                  float phase, float height, u32 color);
    void drawToast();
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
    db::ThumbCache  m_thumbs;
    CoreRegistry    m_cores;
    FrontendSnapshot m_snapshot;

    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<Scene> m_pending;
    ui::Tween m_sceneFade;          /* 0..1: out then in */
    bool m_fadingOut = false;

    theme::Theme   m_theme;
    theme::Palette m_pal;
    theme::Palette m_themeFrom;
    ui::Tween m_themeFade;

    char m_launchError[96] = {};

    char m_toastMsg[96] = {};
    ui::Tween m_toastTween;

    float m_time = 0.f;
    u32   m_lastUs = 0;

    int  m_batteryPct   = -1;
    bool m_batteryChg   = false;
    int  m_batteryPoll  = 0;
    int  m_batteryWarned = 100;   /* last low-battery threshold announced */

#ifdef RS_DEBUG_OVERLAY
    bool m_showOverlay = false;
#endif
};

}  // namespace rs
