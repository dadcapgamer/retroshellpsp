/** Settings: a sidebar of categories (Appearance, Systems, Performance,
 * Audio, Library, About) beside a panel of that category's rows. Right or X enters
 * the panel, O returns to the sidebar; Left/Right change a value, X
 * activates; L/R switch category anywhere. Values persist immediately.
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
        ROW_UI_SOUNDS,
        ROW_COUNT
    };
    enum Category { CAT_APPEARANCE, CAT_SYSTEMS, CAT_PERFORMANCE, CAT_AUDIO,
                    CAT_LIBRARY, CAT_ABOUT, CAT_COUNT };
    /* Rows of the Systems category are SYS_ROW + db::System id. */
    static constexpr int SYS_ROW = 100;

    /* Rows of the current category, in display order. */
    int categoryRows(int out[16]) const;
    void adjustSystem(App& app, int system, int dir);
    int currentRow() const;
    void switchCategory(int dir);
    void adjust(App& app, int row, int dir);
    void activate(App& app, int row);
    const char* valueText(App& app, int row, char* buf, size_t n) const;

    int  m_cat = CAT_APPEARANCE;
    bool m_inContent = false;   /* focus is on the panel, not the sidebar */
    int  m_index = 0;           /* row within the category */
    float m_scroll = 0.f;       /* first visible row, for long categories */
    std::vector<int> m_systems; /* systems with an emulator installed */
    ui::Tween m_entrance;
    ui::Tween m_catFade;
    std::vector<std::string> m_themes;
    int m_themeIdx = 0;
};

}  // namespace rs
