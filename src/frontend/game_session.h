/** GameSession — the scene that runs an emulator core.
 *
 * Owns the bi-layer launch protocol:
 *   enter:  evict frontend caches → (PRX mode: release arena, load core
 *           module, re-reserve arena) → load ROM into the arena → restore
 *           SRAM → run
 *   exit:   flush SRAM → unload ROM & core → restore arena → reload theme
 *           assets → return to Home with the browser state intact
 *
 * The in-game menu (L+R+SELECT) pauses emulation and offers save states
 * with thumbnails, reset, screenshot and exit.
 */
#pragma once

#include "frontend/core_manager.h"
#include "frontend/core_options.h"
#include "frontend/launch_notice.h"
#include "core_api/rs_pause_menu.h"
#include "frontend/database/game_index.h"
#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"
#include "platform/psp/gu_renderer.h"
#include "platform/psp/input_pad.h"
#include "runtime/frame_probe.h"
#include "runtime/save_manager.h"

namespace rs {

class GameSession : public Scene {
public:
    GameSession(db::GameEntry game, std::string coreName)
        : m_game(std::move(game)), m_coreName(std::move(coreName)) {}

    void enter(App& app) override;
    void shutdown(App& app) override;
    void systemSuspend(App& app) override;
    void systemResume(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    enum class State { Starting, Running, Menu, Failed, Exiting };
    bool startCore(App& app);
    void teardown(App& app, bool restoreFrontend);
    void exitToHome(App& app);
    void updateRunning(App& app, float dt);
    void queuePeriodicSram();
    void finishPeriodicSram();
    static int sramWriter(void* arg);
    void updateMenu(App& app);
    /* Emulator Settings: a panel inside the pause menu. */
    void openSettings();
    void updateSettings(App& app);
    void drawSettings(App& app, u32 alpha);
    void drawConfirm(App& app, u32 alpha);
    void updateConfirm(App& app);
    void applyLiveOption(const coreopt::Option& o, int value);
    bool restartPending() const;
    bool saveToSlot(int slot);
    bool slotFromThisCore(int slot) const;
    void setScope(App& app, bool game);
    void flushVideoOptions();
    void restartWithSettings(App& app, int slot);
    void drawFrame(App& app);
    void drawMenu(App& app);
    void openMenu(App& app);
    void resumeGame(bool discardAudio = false);
    void primeAudio();
    void resetPerfWindow();
    void makeThumb(u16* out) const;
    bool makePreview(std::vector<u16>& out, int& w, int& h) const;
    int  m_loadSlot = -1;          /* state to load once the core is up */
    bool m_settingsOpen = false;
    int  m_settingsRow = 0;
    int  m_optCount = 0;
    const coreopt::Option* m_opts[coreopt::MAX_PER_CORE] = {};
    int  m_optValue[coreopt::MAX_PER_CORE] = {};
    int  m_optStart[coreopt::MAX_PER_CORE] = {};   /* as the game started */
    bool m_scopeGame = false;      /* settings saved for this game only */
    /* Pause menu: "Save to Slot N?" before a save state is written. */
    bool m_saveAsk = false;
    int  m_saveAskRow = 0;         /* 0 Yes, 1 No */
    void drawSaveAsk(App& app, u32 alpha);
    ui::Smooth m_settingsScroll;
    /* Apply & Restart asks whether to save first. */
    bool m_confirmOpen = false;
    int  m_confirmRow = 0;         /* 0 save + restart, 1 restart, 2 cancel */
    int  m_confirmSlot = 0;
    /* Live RetroShell options (core_options.h). */
    int  m_frameSkip = 0;          /* fixed skip N; 0 = Auto */
    int  m_skipPhase = 0;          /* frames skipped since the last drawn */
    u32  m_audioExtra = 0;         /* frames added to the audio watermarks */
    u32  mapButtons(const input::Pad& pad) const;
    bool injectFailure(const char* stage) const;
    launch::Action failureAction(App& app) const;

    db::GameEntry m_game;
    std::string m_coreName;
    std::string m_coreLabel;       /* its display name, for the UI */
    CoreManager m_cores;
    State m_state = State::Starting;

    /* Video options resolved once at launch — reading them per frame means
     * a Memory Stick read + JSON parse every frame, which is invisible in
     * PPSSPP but drops real hardware to a couple of fps. */
    enum class ScaleMode { Fit, FourThree, Stretch, OneToOne };
    static const char* scaleOptionName(ScaleMode mode);
    static const char* scaleDisplayName(ScaleMode mode);
    ScaleMode m_scaleMode = ScaleMode::Fit;
    bool m_nearestFilter = false;
    bool m_videoOptionsDirty = false;

    gfx::Texture m_frameTex;
    u16 m_frameW = 0, m_frameH = 0;
    u32 m_uploadedFrameSequence = UINT32_MAX;

    int m_menuRow = RS_PAUSE_RESUME;
    int m_slot = 0;
    ui::Smooth m_menuPos;
    ui::Smooth m_menuScroll;
    ui::Tween m_menuFade;
    save::SlotInfo m_slots[save::SLOTS];
    gfx::Texture m_thumbTex;
    int m_thumbSlot = -1;          /* slot currently in m_thumbTex */

    bool m_frontendEvicted = false;
    bool m_arenaReady = true;
    bool m_coreSessionStarted = false;
    bool m_romLoaded = false;   /* true once the ROM+SRAM are in the core */
    bool m_teardownComplete = false;
    bool m_systemSuspended = false;
    float m_playSeconds = 0.f;   /* running time only; menu and sleep excluded */
    float m_sramTimer = 0.f;
    int m_sramThread = -1;
    void* m_sramSnapshot = nullptr;
    u32 m_sramSnapshotSize = 0;
    bool m_sramWriteOk = false;
    u32 m_sramWriteUs = 0;

    /* Wall-clock accumulator that paces emulation at the core's native
     * frame rate, decoupled from the display refresh (see updateRunning). */
    float m_emuAccum = 0.f;
    bool m_audioRecovery = false;
    bool m_directFrameTexture = false;

    /* Once-per-second performance sample written to the log — the only way
     * to see real-hardware timing, which PPSSPP can't reproduce. */
    u32   m_perfWindowStartUs = 0;
    u32   m_perfEmuUs   = 0;
    int   m_perfFrames  = 0;
    u32   m_perfRenderedUs = 0;
    u32   m_perfSkippedUs = 0;
    int   m_perfRenderedFrames = 0;
    int   m_perfSkippedFrames = 0;
    static constexpr int PERF_SAMPLES = 120;
    u32   m_perfSamples[PERF_SAMPLES] = {};
    int   m_perfSampleCount = 0;
    u32   m_perfRenderedSamples[PERF_SAMPLES] = {};
    u32   m_perfSkippedSamples[PERF_SAMPLES] = {};
    int   m_perfRenderedSampleCount = 0;
    int   m_perfSkippedSampleCount = 0;
    FrameProbe m_videoProbe;

    char m_error[160] = {};    /* sized to relay CoreManager::error() */
};

}  // namespace rs
