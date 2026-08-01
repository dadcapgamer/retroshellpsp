/** Settings: theme picker (built-ins + themes on the Memory Stick),
 * CPU clocks, UI toggles, library rescan. Values persist immediately.
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
        ROW_CPU_MENU,
        ROW_CPU_GAME,
        ROW_UI_SOUNDS,
        ROW_SHOW_FPS,
        ROW_AUTOSAVE,
        ROW_PSP1000_SAFE,
        ROW_RESCAN,
        ROW_COUNT
    };

    void adjust(App& app, int dir);
    void activate(App& app);
    const char* valueText(App& app, int row, char* buf, size_t n) const;

    int m_row = 0;
    ui::Smooth m_rowPos;
    ui::Smooth m_scroll;
    ui::Tween m_entrance;
    std::vector<std::string> m_themes;
    int m_themeIdx = 0;
};

}  // namespace rs
