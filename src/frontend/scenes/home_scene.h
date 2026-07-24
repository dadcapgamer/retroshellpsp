/** Home: XMB-inspired two-level navigation.
 *
 * Level 1 — horizontal category bar: Recently Played, Favorites, the nine
 * systems, Settings. Level 2 — the vertical game list of the focused
 * category, with a detail panel (box art, metadata, favorites). The two
 * levels blend smoothly: entering the list shrinks the bar to the top of
 * the screen.
 */
#pragma once

#include "frontend/database/game_index.h"
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
    static constexpr int NUM_CATS = 12;  /* recent, favs, 9 systems, settings */

    void rebuildList(App& app);
    void updateCats(App& app);
    void updateList(App& app);
    void drawCatBar(App& app, float focus, float enterA, float slideUp);
    void drawEmptyPanel(App& app, float alpha);
    void drawGameList(App& app, float focus);

    int        m_catIdx = 2;
    ui::Smooth m_catPos;
    ui::Tween  m_entrance;
    ui::Tween  m_pulse;

    /* List state. */
    ui::Smooth m_listFocus;                     /* 0 = cats, 1 = list */
    bool       m_inList = false;
    int        m_listIdx = 0;
    ui::Smooth m_scroll;
    std::vector<const db::GameEntry*> m_visible;
    u32        m_lastIndexCount = 0;            /* refresh detection */
};

}  // namespace rs
