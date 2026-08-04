/** Home: console-first two-level navigation.
 *
 * Level 1 — horizontal console selector plus a recent-games shelf.
 * Level 2 — thumbnail grid with a persistent metadata/box-art preview.
 * Triangle opens quick actions for the selected game without launching it.
 */
#pragma once

#include "frontend/core_registry.h"
#include "frontend/database/game_index.h"
#include "frontend/database/metadata.h"
#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"
#include "rs_common.h"

#include <vector>

namespace rs {

class HomeScene : public Scene {
public:
    void enter(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    static constexpr int NUM_CATS = 11;  /* 9 systems + favorites + settings */

    void rebuildList(App& app);
    void rebuildRecents(App& app);
    void refreshSelection(App& app);   /* schedules deferred stick-backed data */
    void hydrateSelection(App& app);   /* runs only after navigation settles */
    void updateCats(App& app);
    void updateList(App& app);
    void openActions(App& app, const db::GameEntry& game);
    void updateActions(App& app);
    void drawHome(App& app, float alpha, float slide);
    void drawBrowser(App& app, float alpha);
    void drawActions(App& app);

    /* Core picker — opens before launch when a remembered core disappeared,
     * or any time via the Square shortcut when alternatives are installed. */
    void openCorePicker(App& app, const db::GameEntry& game);
    void updatePicker(App& app);
    void drawPicker(App& app);

    int        m_catIdx = 0;
    ui::Smooth m_catPos;
    ui::Tween  m_entrance;

    /* Game-grid state. */
    ui::Smooth m_listFocus;                     /* 0 = cats, 1 = list */
    bool       m_inList = false;
    int        m_listIdx = 0;
    ui::Smooth m_scroll;
    std::vector<const db::GameEntry*> m_visible;
    bool       m_recentFocus = false;
    int        m_recentIdx = 0;
    std::vector<const db::GameEntry*> m_recentVisible;
    u32        m_lastIndexGen = 0;    /* index generation, not count — a
                                       * count-preserving rescan still frees
                                       * the GameEntry* held in m_visible */

    /* Core info for the focused game, cached because resolving reads the
     * per-game config file — too costly for the 60fps draw path. */
    const CoreInfo* m_selCore = nullptr;
    bool            m_selMultiCore = false;
    db::GameMeta    m_selMeta;      /* cached; loadMeta reads the stick */
    float           m_selectionSettle = 0.f;
    u32             m_hydratedHash = 0;

    /* Selected-game quick actions. Save-state headers are queried once when
     * the panel opens, never in the 60 fps draw path. */
    bool      m_actionsOpen = false;
    int       m_actionIdx = 0;
    int       m_saveCount = 0;
    db::GameEntry m_actionGame;
    ui::Tween m_actionsFade;

    /* Core picker state. The game is copied so a background scan can't
     * invalidate it while the modal is up; m_pickerCurrent marks the row
     * a plain launch would use. */
    bool          m_pickerOpen = false;
    int           m_pickerIdx = 0;
    db::GameEntry m_pickerGame;
    std::vector<const CoreInfo*> m_pickerCores;
    const CoreInfo* m_pickerCurrent = nullptr;
    ui::Tween     m_pickerFade;
};

}  // namespace rs
