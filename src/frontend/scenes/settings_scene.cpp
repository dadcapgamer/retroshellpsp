#include "frontend/scenes/settings_scene.h"

#include "core_api/rs_core_api.h"

#include "rs_build_stamp.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "frontend/scenes/setup_scene.h"
#include "frontend/ui/chrome.h"
#include "frontend/ui/icons.h"
#include "platform/psp/power.h"
#include "runtime/config.h"

#include <pspctrl.h>

#include <cstdio>
#include <cstring>

namespace rs {

namespace {
namespace L = ui::layout;
using ui::fade;

const char* ROW_LABELS[] = {
    "Theme", "Accent color", "Time format", "Artwork", "Menu CPU clock",
    "In-game CPU clock", "Show FPS", "Auto-save", "Rescan library",
    "Run setup again", "UI sounds",
};
struct CategoryDef {
    const char* label;
    const char* title;
    ui::Icon icon;
    int rows[5];          /* -1 terminated */
};
const CategoryDef CATS[] = {
    {"Appearance", "APPEARANCE", ui::Icon::Gear, {0, 1, 2, 3, -1}},
    {"Performance", "PERFORMANCE", ui::Icon::Gauge, {4, 5, 6, 7, -1}},
    {"Audio", "AUDIO", ui::Icon::Speaker, {10, -1, -1, -1, -1}},
    {"Library", "LIBRARY", ui::Icon::Library, {8, 9, -1, -1, -1}},
    {"About", "ABOUT", ui::Icon::Info, {-1, -1, -1, -1, -1}},
};

constexpr int CPU_STEPS[] = {222, 266, 333};
constexpr float TOP = 36.f;
constexpr float SIDE_X = L::MARGIN, SIDE_W = 160.f, SIDE_ROW = 24.f;
constexpr float PANEL_X = 184.f, PANEL_W = L::RIGHT - PANEL_X;
constexpr float PANEL_H = L::CONTENT_BOTTOM - TOP;
constexpr float ROW_H = 22.f;

int cpuStepIndex(int mhz) {
    for (int i = 0; i < 3; i++)
        if (CPU_STEPS[i] == mhz) return i;
    return 0;
}

bool isAction(int row) { return row == 8 || row == 9; }
}  // namespace

void SettingsScene::enter(App& app) {
    m_cat = CAT_APPEARANCE;
    m_inContent = false;
    m_index = 0;
    m_entrance.start(0.18f);
    m_catFade.t = 1.f;
    m_themes = theme::availableThemes();
    m_themeIdx = 0;
    for (size_t i = 0; i < m_themes.size(); i++)
        if (m_themes[i] == app.theme().id) m_themeIdx = int(i);
}

int SettingsScene::categoryRows(int out[6]) const {
    int n = 0;
    for (int r : CATS[m_cat].rows)
        if (r >= 0) out[n++] = r;
    return n;
}

int SettingsScene::currentRow() const {
    if (!m_inContent) return -1;
    int rows[6];
    const int n = categoryRows(rows);
    return n ? rows[rsClamp(m_index, 0, n - 1)] : -1;
}

void SettingsScene::switchCategory(int dir) {
    const int next = rsClamp(m_cat + dir, 0, CAT_COUNT - 1);
    if (next == m_cat) return;
    m_cat = next;
    m_index = 0;
    int rows[6];
    if (categoryRows(rows) == 0) m_inContent = false;
    m_catFade.start(0.12f);
}

void SettingsScene::adjust(App& app, int row, int dir) {
    auto& c = cfg::get();
    switch (row) {
        case ROW_THEME: {
            m_themeIdx =
                (m_themeIdx + dir + int(m_themes.size())) % int(m_themes.size());
            app.setThemeById(m_themes[size_t(m_themeIdx)]);
            break;
        }
        case ROW_ACCENT: {
            const int next =
                (c.accent + dir + theme::ACCENT_COUNT) % theme::ACCENT_COUNT;
            app.setAccentIndex(next);
            break;
        }
        case ROW_TIME_FORMAT:
            c.clock24Hour = !c.clock24Hour;
            cfg::save();
            break;
        case ROW_ARTWORK:
            c.showArt = !c.showArt;
            cfg::save();
            break;
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

void SettingsScene::activate(App& app, int row) {
    if (row == ROW_SETUP) {
        app.switchScene(std::make_unique<SetupScene>(/*fromSettings=*/true));
    } else if (row == ROW_RESCAN) {
        if (!app.scanner().running()) {
            app.scanner().start();
            app.toast("Rescanning library...");
        }
    } else {
        adjust(app, row, 1);
    }
}

const char* SettingsScene::valueText(App& app, int row, char* buf,
                                     size_t n) const {
    const auto& c = cfg::get();
    switch (row) {
        case ROW_THEME:
            std::snprintf(buf, n, "%s", app.theme().title.c_str());
            return buf;
        case ROW_TIME_FORMAT: return c.clock24Hour ? "24-hour" : "12-hour";
        case ROW_ARTWORK:   return c.showArt ? "On" : "Text only";
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
            return app.scanner().running() ? "Scanning..." : "Press X";
        case ROW_SETUP:     return "Press X";
        default: return "";
    }
}

void SettingsScene::update(App& app, float dt) {
    const auto& pad = app.pad();
    int rows[6];
    const int n = categoryRows(rows);

    if (pad.isPressed(PSP_CTRL_LTRIGGER)) switchCategory(-1);
    if (pad.isPressed(PSP_CTRL_RTRIGGER)) switchCategory(1);
    if (pad.isPressed(PSP_CTRL_START)) {
        app.switchScene(std::make_unique<HomeScene>());
        return;
    }

    if (!m_inContent) {
        if (pad.navPressed(PSP_CTRL_UP)) switchCategory(-1);
        if (pad.navPressed(PSP_CTRL_DOWN)) switchCategory(1);
        if ((pad.navPressed(PSP_CTRL_RIGHT) || pad.isPressed(PSP_CTRL_CROSS)) &&
            n > 0) {
            m_inContent = true;
            m_index = 0;
        }
        /* O goes back; Triangle (which opened Settings) closes it too. */
        if (pad.isPressed(PSP_CTRL_CIRCLE) || pad.isPressed(PSP_CTRL_TRIANGLE))
            app.switchScene(std::make_unique<HomeScene>());
    } else {
        if (pad.navPressed(PSP_CTRL_UP) && m_index > 0) m_index--;
        if (pad.navPressed(PSP_CTRL_DOWN) && m_index < n - 1) m_index++;
        const int row = currentRow();
        if (pad.navPressed(PSP_CTRL_LEFT)) adjust(app, row, -1);
        if (pad.navPressed(PSP_CTRL_RIGHT)) adjust(app, row, 1);
        if (pad.isPressed(PSP_CTRL_CROSS)) activate(app, row);
        if (pad.isPressed(PSP_CTRL_CIRCLE)) m_inContent = false;
        if (pad.isPressed(PSP_CTRL_TRIANGLE))
            app.switchScene(std::make_unique<HomeScene>());
    }

    m_entrance.update(dt);
    m_catFade.update(dt);
}

void SettingsScene::draw(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();

    app.drawBackground();
    app.drawTopBar();

    const float enter = ui::easeOutCubic(m_entrance.t);
    const u32 a = u32(enter * 255.f);
    const float dy = ui::snap((1.f - enter) * 12.f);
    const float cf = ui::easeOutCubic(m_catFade.t);
    const u32 pa = u32(float(a) * cf);
    const float pdy = ui::snap((1.f - cf) * 6.f);

    /* Sidebar: icon + label per category, hairlines between. The active
     * category takes the focus fill while the sidebar has focus; once the
     * panel has it, the category stays marked with a quiet surface and an
     * accent edge, so blue still means exactly where input goes. */
    for (int c = 0; c < CAT_COUNT; c++) {
        const float y = TOP + float(c) * SIDE_ROW + dy;
        const bool active = c == m_cat;
        ui::RowStyle style;
        style.focused = active;
        style.soft = active && m_inContent;
        style.chevron = true;
        style.strong = active;
        style.icon = int(CATS[c].icon);
        ui::menuRow(app, SIDE_X, y, SIDE_W, SIDE_ROW, CATS[c].label, nullptr,
                    style, a);
        if (style.soft)
            r.rect(SIDE_X, y + 3.f, 2.f, SIDE_ROW - 6.f, fade(pal.focusEdge, a));
        if (!active && c + 1 < CAT_COUNT && c + 1 != m_cat)
            ui::rowRule(app, SIDE_X, y + SIDE_ROW - 1.f, SIDE_W, a);
    }

    /* Panel: the category's title and rows. */
    ui::card(app, PANEL_X, TOP + dy, PANEL_W, PANEL_H, a);
    const float px = PANEL_X + 6.f, pw = PANEL_W - 12.f;
    fonts.title.draw(r, PANEL_X + 14.f,
                     fonts.title.centerY(TOP + 10.f + dy + pdy, 10.f),
                     CATS[m_cat].title, fade(pal.textPrimary, pa),
                     text::Align::Left, 1.f);

    float y = TOP + 30.f + dy + pdy;
    const int focusedRow = currentRow();
    char buf[48];
    int rows[6];
    const int n = categoryRows(rows);
    for (int i = 0; i < n; i++, y += ROW_H) {
        const int row = rows[i];
        ui::RowStyle style;
        style.focused = row == focusedRow;
        style.adjustable = !isAction(row) && row != ROW_ACCENT;
        const bool swatches = row == ROW_ACCENT;
        ui::menuRow(app, px, y, pw, ROW_H, ROW_LABELS[row],
                    swatches ? nullptr : valueText(app, row, buf, sizeof buf),
                    style, pa);
        if (swatches) {
            /* Accent swatches in place of a value, the active one ringed. */
            const float cy = y + ROW_H * .5f;
            for (int sw = 0; sw < theme::ACCENT_COUNT; sw++) {
                const float cx = px + pw - 16.f -
                                 float(theme::ACCENT_COUNT - 1 - sw) * 17.f;
                if (sw == cfg::get().accent)
                    ui::prim::ring(r, cx, cy, 7.f,
                                   fade(style.focused ? pal.onAccent
                                                      : pal.textPrimary, pa));
                ui::prim::circle(r, cx, cy, 4.5f,
                                 fade(rsHex(theme::accentOption(sw).rgb), pa));
            }
        }
        if (!style.focused && i + 1 < n && rows[i + 1] != focusedRow)
            ui::rowRule(app, px, y + ROW_H - 1.f, pw, pa);
    }

    if (m_cat == CAT_ABOUT) {
        /* Build identity: testers report against a build, and a mismatched
         * EBOOT/core set fails with an API version error that is otherwise
         * hard to read. */
        char api[16];
        std::snprintf(api, sizeof api, "v%u", unsigned(RS_CORE_API_VERSION));
        const char* keys[] = {"Version", "Built", "Core API"};
        char built[40];
        std::snprintf(built, sizeof built, "%s UTC", RS_BUILD_STAMP);
        const char* values[] = {RS_RELEASE_VERSION, built, api};
        for (int i = 0; i < 3; i++, y += ROW_H) {
            fonts.body.draw(r, px + 10.f, fonts.body.centerY(y, ROW_H), keys[i],
                            fade(pal.textSecondary, pa));
            fonts.body.draw(r, px + pw - 10.f, fonts.body.centerY(y, ROW_H),
                            values[i], fade(pal.textPrimary, pa),
                            text::Align::Right);
            if (i < 2) ui::rowRule(app, px, y + ROW_H - 1.f, pw, pa);
        }
    }

    using B = ui::prim::Button;
    if (!m_inContent) {
        const App::Hint hints[] = {
            {B::Cross, "Select"}, {B::Circle, "Back"}, {B::Start, "Home"},
        };
        app.drawHintBar(hints, 3);
    } else {
        const App::Hint hints[] = {
            {isAction(focusedRow) ? B::Cross : B::DpadLeftRight,
             isAction(focusedRow) ? "Select" : "Change"},
            {B::Circle, "Back"}, {B::Start, "Home"},
        };
        app.drawHintBar(hints, 3);
    }
}

}  // namespace rs
