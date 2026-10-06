/** Settings: three tabs — Appearance, Performance, System — each a short,
 * labelled list. L/R (or Up onto the tab bar, then Left/Right) changes tab;
 * Left/Right changes a value; X activates. Values persist immediately.
 */
#pragma once

#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"
#include "rs_common.h"

#include <string>
#include <vector>

namespace rs {

class SettingsScene : public Scene {
public:
    void enter(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    enum Row {
        ROW_THEME = 0,
        ROW_ACCENT,
        ROW_TIME_FORMAT,
        ROW_ARTWORK,
        ROW_CPU_MENU,
        ROW_CPU_GAME,
        ROW_SHOW_FPS,
        ROW_AUTOSAVE,
        ROW_RESCAN,
        ROW_SETUP,
        ROW_UI_SOUNDS,
        ROW_COUNT
    };
    enum Tab { TAB_APPEARANCE, TAB_PERFORMANCE, TAB_SYSTEM, TAB_COUNT };

    /* Rows of the current tab, in display order; -1 entries are not used. */
    int tabRows(int out[6]) const;
    int currentRow() const;
    void switchTab(int dir);
    void adjust(App& app, int row, int dir);
    void activate(App& app, int row);
    const char* valueText(App& app, int row, char* buf, size_t n) const;

    int m_tab = TAB_APPEARANCE;
    int m_index = 0;            /* position within the tab; -1 = the tab bar */
    ui::Tween m_entrance;
    ui::Tween m_tabFade;
    int m_tabDir = 0;
    std::vector<std::string> m_themes;
    int m_themeIdx = 0;
};

}  // namespace rs
