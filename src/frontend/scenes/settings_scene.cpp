#include "frontend/scenes/settings_scene.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "platform/psp/power.h"
#include "runtime/config.h"

#include <pspctrl.h>

#include <cstdio>
#include <cstring>

namespace rs {

namespace {
const char* ROW_LABELS[] = {
    "Theme", "Menu CPU clock", "In-game CPU clock", "UI sounds",
    "Show FPS", "Auto-save", "Rescan library",
};
constexpr int CPU_STEPS[] = {222, 266, 333};

int cpuStepIndex(int mhz) {
    for (int i = 0; i < 3; i++)
        if (CPU_STEPS[i] == mhz) return i;
    return 0;
}
}  // namespace

void SettingsScene::enter(App& app) {
    m_row = 0;
    m_rowPos.snap(0.f);
    m_entrance.start(0.35f);
    m_themes = theme::availableThemes();
    m_themeIdx = 0;
    for (size_t i = 0; i < m_themes.size(); i++)
        if (m_themes[i] == app.theme().id) m_themeIdx = int(i);
}

void SettingsScene::adjust(App& app, int dir) {
    auto& c = cfg::get();
    switch (m_row) {
        case ROW_THEME: {
            m_themeIdx =
                (m_themeIdx + dir + int(m_themes.size())) % int(m_themes.size());
            app.setThemeById(m_themes[size_t(m_themeIdx)]);
            break;
        }
        case ROW_CPU_MENU: {
            const int i = rsClamp(cpuStepIndex(c.cpuMenuMhz) + dir, 0, 2);
            c.cpuMenuMhz = CPU_STEPS[i];
            power::setCpuMhz(c.cpuMenuMhz);
            cfg::save();
            break;
        }
        case ROW_CPU_GAME: {
            const int i = rsClamp(cpuStepIndex(c.cpuGameMhz) + dir, 0, 2);
            c.cpuGameMhz = CPU_STEPS[i];
            cfg::save();
            break;
        }
        case ROW_UI_SOUNDS:
            c.uiSounds = !c.uiSounds;
            cfg::save();
            break;
        case ROW_SHOW_FPS:
            c.showFps = !c.showFps;
            cfg::save();
            break;
        case ROW_AUTOSAVE:
            c.autosave = !c.autosave;
            cfg::save();
            break;
        default:
            break;
    }
}

void SettingsScene::activate(App& app) {
    if (m_row == ROW_RESCAN) {
        if (!app.scanner().running()) {
            app.scanner().start();
            app.toast("Rescanning library…");
        }
    } else {
        adjust(app, 1);
    }
}

const char* SettingsScene::valueText(App& app, int row, char* buf,
                                     size_t n) const {
    const auto& c = cfg::get();
    switch (row) {
        case ROW_THEME:
            std::snprintf(buf, n, "%s", app.theme().title.c_str());
            return buf;
        case ROW_CPU_MENU:
            std::snprintf(buf, n, "%d MHz", c.cpuMenuMhz);
            return buf;
        case ROW_CPU_GAME:
            std::snprintf(buf, n, "%d MHz", c.cpuGameMhz);
            return buf;
        case ROW_UI_SOUNDS: return c.uiSounds ? "On" : "Off";
        case ROW_SHOW_FPS:  return c.showFps ? "On" : "Off";
        case ROW_AUTOSAVE:  return c.autosave ? "On" : "Off";
        case ROW_RESCAN:
            return app.scanner().running() ? "Scanning…" : "Press ×";
        default: return "";
    }
}

void SettingsScene::update(App& app, float dt) {
    const auto& pad = app.pad();
    if (pad.navPressed(PSP_CTRL_UP) && m_row > 0) m_row--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_row < ROW_COUNT - 1) m_row++;
    if (pad.navPressed(PSP_CTRL_LEFT)) adjust(app, -1);
    if (pad.navPressed(PSP_CTRL_RIGHT)) adjust(app, 1);
    if (pad.isPressed(PSP_CTRL_CROSS)) activate(app);
    if (pad.isPressed(PSP_CTRL_CIRCLE))
        app.switchScene(std::make_unique<HomeScene>());

    m_rowPos.to(float(m_row));
    m_rowPos.update(dt, 14.f);
    m_entrance.update(dt);
}

void SettingsScene::draw(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();

    app.drawBackground();
    app.drawTopBar();

    const float enter = ui::easeOutCubic(m_entrance.t);
    const u32 a = u32(enter * 255.f);
    const float slide = (1.f - enter) * 14.f;

    fonts.large.drawShadow(r, 24.f, 30.f + slide, "Settings",
                           rsWithAlpha(pal.textPrimary, a), pal.shadow);

    const float px = 24.f, pw = 432.f, py = 62.f + slide;
    const float rowH = 24.f;

    ui::prim::roundedRect(r, px, py, pw, rowH * ROW_COUNT + 16.f, 12.f,
                          rsWithAlpha(pal.panelBg,
                                      rsAlphaOf(pal.panelBg) * a / 255u));
    ui::prim::roundedOutline(r, px, py, pw, rowH * ROW_COUNT + 16.f, 12.f,
                             rsWithAlpha(pal.panelOutline,
                                         rsAlphaOf(pal.panelOutline) * a /
                                             255u));

    /* Sliding highlight. */
    const float hy = py + 8.f + m_rowPos.v * rowH;
    ui::prim::roundedRect(r, px + 8.f, hy, pw - 16.f, rowH - 2.f, 8.f,
                          rsWithAlpha(pal.tileFocusBg,
                                      rsAlphaOf(pal.tileFocusBg) * a / 255u));
    r.rect(px + 12.f, hy + 4.f, 3.f, rowH - 10.f, rsWithAlpha(pal.accent, a));

    char buf[48];
    for (int i = 0; i < ROW_COUNT; i++) {
        const float y = py + 8.f + float(i) * rowH + 3.f;
        const bool sel = i == m_row;
        fonts.body.draw(r, px + 24.f, y, ROW_LABELS[i],
                        rsWithAlpha(sel ? pal.textPrimary : pal.textSecondary,
                                    a));
        fonts.body.draw(r, px + pw - 24.f, y, valueText(app, i, buf, sizeof buf),
                        rsWithAlpha(sel ? pal.accent : pal.textDim, a),
                        text::Align::Right);
    }

    const App::Hint hints[] = {
        {ui::prim::Button::Cross, "Change"},
        {ui::prim::Button::Circle, "Back"},
    };
    app.drawHintBar(hints, 2);
}

}  // namespace rs
