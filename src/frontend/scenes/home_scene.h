/** Home: one coherent shell with four spatial layers.
 *
 *   Continue Playing   (above)   recent games across systems
 *   Systems            (home)    horizontal rail, change system with L/R
 *   Library            (below)   vertical game list for the selected system
 *   Game Detail        (deeper)  focused single-game view
 *
 * Horizontal always changes system, vertical moves within or between
 * layers, X plays (Library, Continue) or enters (Systems), O goes back,
 * Square favorites, Triangle opens options — whose first rows are Play and
 * Game Details — and Start returns Home. Select, in the Library only, opens the secondary
 * View menu (filter, sort, search) — kept off the primary path on purpose.
 * The navigation rules live in home_nav.h so they are unit-testable; this
 * class owns input, data and painting.
 */
#pragma once

#include "frontend/core_registry.h"
#include "frontend/database/game_index.h"
#include "frontend/database/library_view.h"
#include "frontend/database/metadata.h"
#include "frontend/launch_notice.h"
#include "frontend/scenes/home_nav.h"
#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"
#include "platform/psp/gu_renderer.h"
#include "runtime/save_manager.h"
#include "rs_common.h"

#include <vector>

namespace rs {

class HomeScene : public Scene {
public:
    void enter(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;
    void shutdown(App& app) override;

private:
    enum class Overlay : u8 { None, CorePicker, ViewMenu, Search, Error };

    /* View menu rows; Clear only exists while a search is active. */
    enum ViewRow : int { VR_SHOW, VR_SORT, VR_SEARCH, VR_CLEAR, VR_BACK };

    /* Game Detail actions. The list is built per game: Emulator appears only
     * when the system has a choice, Remove from Continue only when the game
     * is in it. Cheats and manuals have no data source yet, so they are not
     * offered rather than offered and refused. */
    enum DetailAction : int {
        DA_PLAY, DA_STATES, DA_FAVORITE, DA_EMULATOR, DA_DELETE_SAVE,
        DA_REMOVE_RECENT
    };
    int  detailActions(int out[8]) const;

    /* --- data ------------------------------------------------------------ */
    void rebuildSystems(App& app);
    void rebuildList(App& app);
    void rebuildRecents(App& app);
    void applyView(App& app, db::ViewState view, const std::string& query);
    void refreshListKeepingPosition(App& app);
    void syncActiveSystem();
    const db::GameEntry* focusedGame() const;
    int  currentSystemId() const;
    int  gamesInCurrent() const { return int(m_visible.size()); }
    void noteSelection(App& app);
    void hydrateSelection(App& app);
    const db::GameMeta* cachedMeta(u32 hash) const;

    /* --- input ----------------------------------------------------------- */
    void updateSystems(App& app);
    void updateContinue(App& app);
    void updateLibrary(App& app);
    void updateDetail(App& app);
    void updatePicker(App& app);
    void switchSystem(App& app, int dir);
    void launch(App& app, const db::GameEntry& game);
    void toggleFavorite(App& app, const db::GameEntry& game);
    void openDetail(App& app, const db::GameEntry& game);
    void activateDetail(App& app, int row);
    void openCorePicker(App& app, const db::GameEntry& game);
    void openSettings(App& app);
    void checkLaunchError(App& app, const db::GameEntry& game);
    void openError(App& app, const db::GameEntry& game, const char* raw);
    void updateError(App& app);
    void openViewMenu(App& app);
    void updateViewMenu(App& app);
    int  viewRows(int out[5]) const;
    void openSearch(App& app);
    void updateSearch(App& app);
    void recountSearch(App& app);

    /* --- drawing --------------------------------------------------------- */
    void drawSystems(App& app, u32 alpha, float dy);
    void drawContinue(App& app, u32 alpha, float dy);
    void drawLibrary(App& app, u32 alpha, float dy);
    void drawDetail(App& app, u32 alpha, float dy);
    void drawMetaLine(App& app, const db::GameEntry& g, float x, float capsTop,
                      float maxW, u32 alpha);
    void drawStates(App& app, u32 alpha, float dy);
    void openStates(App& app);
    void updateStates(App& app, float dt);
    void closeStates();
    void loadStatePreview(App& app);
    void drawPicker(App& app);
    void drawViewMenu(App& app);
    void drawSearch(App& app);
    void drawError(App& app);
    void drawLegend(App& app);

    /* --- state ----------------------------------------------------------- */
    nav::HomeNav m_nav;
    Overlay      m_overlay = Overlay::None;

    std::vector<int> m_systems;                       /* db::System ids on the rail */
    std::vector<const db::GameEntry*> m_visible;      /* current system's games */
    std::vector<const db::GameEntry*> m_recents;      /* Continue Playing */
    db::ViewState m_view;                  /* filter + sort of the current system */
    std::string   m_query;                 /* active search, Library only */
    int  m_systemTotal = 0;                /* all games in the system, unfiltered */
    bool m_empty = false;                  /* no games anywhere: show setup help */
    bool m_allHidden = false;              /* games exist, every system is off */
    bool m_romRootMissing = false;
    bool m_listDirty = false;              /* favorites changed under a filter */
    u32 m_lastIndexGen = 0;   /* index generation, not count — a count-preserving
                               * rescan still frees the GameEntry* held above */
    u32 m_recentsRevision = 0;
    u64 m_now = 0;            /* local timestamp, refreshed about once a second */
    float m_nowTimer = 0.f;

    /* Motion. layerPos: Continue = -1, Systems = 0, Library = 1, Detail = 2. */
    ui::Smooth m_layerPos;
    ui::Smooth m_railPos;       /* selected rail slot, fractional */
    ui::Smooth m_railFirst;     /* first visible rail slot, fractional */
    ui::Smooth m_contScroll;
    ui::Smooth m_scroll;        /* Library list scroll, in rows */
    ui::Smooth m_slideX;        /* horizontal slide-in after a system switch */
    ui::Tween  m_entrance;
    ui::Tween  m_titleFade;     /* system name crossfade after a switch */
    int        m_titleDir = 0;

    /* Deferred, stick-backed data for the focused game. */
    const CoreInfo* m_selCore = nullptr;
    bool            m_selMultiCore = false;
    db::GameMeta    m_selMeta;
    u32             m_trackedHash = 0;
    u32             m_hydratedHash = 0;
    float           m_selectionSettle = 0.f;
    struct MetaEntry { u32 hash = 0; db::GameMeta meta; };
    MetaEntry       m_metaCache[8];
    int             m_metaNext = 0;

    /* Detail layer. The game is copied so a background scan can't free it
     * while the layer is open. */
    db::GameEntry m_detailGame;
    bool          m_detailHasSave = false;
    int           m_detailRow = 0;
    bool          m_detailInRecents = false;
    bool          m_confirmDelete = false;   /* Delete Save Data: second X */
    float         m_detailScroll = 0.f;      /* first visible action row */

    /* Save States, inside Game Detail: the slots, the selected one's full
     * preview (loaded once the selection settles), and a two-press delete. */
    bool            m_states = false;
    int             m_detailStateCount = 0;
    int             m_stateIdx = 0;
    save::SlotInfo  m_stateSlots[save::SLOTS];
    gfx::Texture    m_preview;
    int             m_previewW = 0, m_previewH = 0;
    int             m_previewSlot = -1;
    bool            m_previewLegacy = false;   /* header thumbnail only */
    float           m_previewSettle = 0.f;
    bool            m_confirmStateDelete = false;
    float           m_dt = 0.f;               /* this frame's delta, for layers */

    ui::Tween     m_overlayFade;

    /* View menu and search entry. */
    int           m_viewRow = 0;
    std::string   m_searchBuf;
    int           m_searchChar = 0;        /* index into the search alphabet */
    bool          m_searchTouched = false; /* pending letter chosen but not locked */
    int           m_searchCount = 0;       /* live match count for the preview */

    /* Launch failure shown as an actionable error state. */
    launch::Notice m_notice;
    std::string    m_noticeMessage;
    db::GameEntry  m_noticeGame;

    /* Core picker (Game Details -> Core). */
    int           m_pickerIdx = 0;
    db::GameEntry m_pickerGame;
    std::vector<const CoreInfo*> m_pickerCores;
    const CoreInfo* m_pickerCurrent = nullptr;
};

}  // namespace rs
