#include "frontend/scenes/settings_scene.h"

#include "core_api/rs_core_api.h"

#include "rs_build_stamp.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "frontend/scenes/setup_scene.h"
#include "frontend/ui/chrome.h"
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
    "Theme", "Accent", "Time format", "Artwork", "Menu CPU clock",
    "In-game CPU clock", "Show FPS", "Auto-save", "Rescan library",
    "Run setup again", "UI sounds",
};
const char* TAB_LABELS[] = {"Appearance", "Performance", "System"};

/* Each tab is two labelled sections. */
struct Section { const char* label; int rows[3]; };
struct TabDef { Section sections[2]; };
const TabDef TABS[] = {
    {{{"THEME", {0, 1, -1}}, {"DISPLAY", {2, 3, -1}}}},
    {{{"CPU CLOCK", {4, 5, -1}}, {"IN GAME", {6, 7, -1}}}},
    {{{"LIBRARY", {8, 9, -1}}, {"SOUND", {10, -1, -1}}}},
};

constexpr int CPU_STEPS[] = {222, 266, 333};
constexpr float TAB_Y = 34.f, TAB_H = 20.f;
constexpr float RULE_Y = TAB_Y + TAB_H + 1.f;
constexpr float BODY_Y = 64.f;
constexpr float SECTION_GAP = 8.f;
constexpr float LABEL_H = 14.f;

int cpuStepIndex(int mhz) {
    for (int i = 0; i < 3; i++)
        if (CPU_STEPS[i] == mhz) return i;
    return 0;
}

bool isAction(int row) { return row == 8 || row == 9; }
}  // namespace

void SettingsScene::enter(App& app) {
    m_tab = TAB_APPEARANCE;
    m_index = 0;
    m_entrance.start(0.18f);
    m_tabFade.t = 1.f;
    m_themes = theme::availableThemes();
    m_themeIdx = 0;
    for (size_t i = 0; i < m_themes.size(); i++)
        if (m_themes[i] == app.theme().id) m_themeIdx = int(i);
}

int SettingsScene::tabRows(int out[6]) const {
    int n = 0;
    for (const Section& sec : TABS[m_tab].sections)
        for (int r : sec.rows)
            if (r >= 0) out[n++] = r;
    return n;
}

int SettingsScene::currentRow() const {
    if (m_index < 0) return -1;
    int rows[6];
    const int n = tabRows(rows);
    return rows[rsClamp(m_index, 0, n - 1)];
}

void SettingsScene::switchTab(int dir) {
    const int next = rsClamp(m_tab + dir, 0, TAB_COUNT - 1);
    if (next == m_tab) return;
    m_tab = next;
    if (m_index >= 0) m_index = 0;
    m_tabDir = dir;
    m_tabFade.start(0.13f);
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
            for (char* p = buf; *p; ++p)
                if (*p >= 'a' && *p <= 'z') *p = char(*p - 32);
            return buf;
        case ROW_TIME_FORMAT: return c.clock24Hour ? "24-HOUR" : "12-HOUR";
        case ROW_ARTWORK:   return c.showArt ? "ON" : "TEXT ONLY";
        case ROW_CPU_MENU:
            std::snprintf(buf, n, "%d MHZ", c.cpuMenuMhz);
            return buf;
        case ROW_CPU_GAME:
            std::snprintf(buf, n, "%d MHZ", c.cpuGameMhz);
            return buf;
        case ROW_UI_SOUNDS: return c.uiSounds ? "ON" : "OFF";
        case ROW_SHOW_FPS:  return c.showFps ? "ON" : "OFF";
        case ROW_AUTOSAVE:  return c.autosave ? "ON" : "OFF";
        case ROW_RESCAN:
            return app.scanner().running() ? "SCANNING..." : "PRESS X";
        case ROW_SETUP:     return "PRESS X";
        default: return "";
    }
}

void SettingsScene::update(App& app, float dt) {
    const auto& pad = app.pad();
    int rows[6];
    const int n = tabRows(rows);

    if (pad.isPressed(PSP_CTRL_LTRIGGER)) switchTab(-1);
    if (pad.isPressed(PSP_CTRL_RTRIGGER)) switchTab(1);
    if (pad.navPressed(PSP_CTRL_UP) && m_index >= 0) m_index--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_index < n - 1) m_index++;

    if (m_index < 0) {
        /* Tab bar focused: Left/Right switch category, X/Down enters it. */
        if (pad.navPressed(PSP_CTRL_LEFT)) switchTab(-1);
        if (pad.navPressed(PSP_CTRL_RIGHT)) switchTab(1);
        if (pad.isPressed(PSP_CTRL_CROSS)) m_index = 0;
    } else {
        const int row = currentRow();
        if (pad.navPressed(PSP_CTRL_LEFT)) adjust(app, row, -1);
        if (pad.navPressed(PSP_CTRL_RIGHT)) adjust(app, row, 1);
        if (pad.isPressed(PSP_CTRL_CROSS)) activate(app, row);
    }
    /* O goes back; Start jumps Home like everywhere else. */
    if (pad.isPressed(PSP_CTRL_CIRCLE) || pad.isPressed(PSP_CTRL_START) ||
        pad.isPressed(PSP_CTRL_TRIANGLE))
        app.switchScene(std::make_unique<HomeScene>());

    m_entrance.update(dt);
    m_tabFade.update(dt);
}

void SettingsScene::draw(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();

    app.drawBackground();
    app.drawTopBar("SETTINGS");

    /* Arrives from below like the Library; a tab switch slides its body in
     * from the direction of travel. */
    const float enter = ui::easeOutCubic(m_entrance.t);
    const u32 a = u32(enter * 255.f);
    const float dy = ui::snap((1.f - enter) * 16.f);
    const float tf = ui::easeOutCubic(m_tabFade.t);
    const u32 ba = u32(float(a) * tf);
    const float bdx = ui::snap((1.f - tf) * 14.f * float(m_tabDir));

    /* Tabs: the active one carries the accent underline; with the tab bar
     * itself focused it takes the focus fill instead. */
    float tx = L::MARGIN;
    for (int t = 0; t < TAB_COUNT; t++) {
        const bool active = t == m_tab;
        const text::Font& face = active ? fonts.bodyStrong : fonts.body;
        const float w = face.measure(TAB_LABELS[t]);
        const float pillW = w + 20.f;
        u32 ink = active ? pal.textPrimary : pal.textMuted;
        if (active && m_index < 0) {
            ui::focusFill(app, tx, TAB_Y + dy, pillW, TAB_H, a);
            ink = pal.onAccent;
        }
        face.draw(r, tx + 10.f, face.centerY(TAB_Y + dy, TAB_H), TAB_LABELS[t],
                  fade(ink, a));
        if (active)
            ui::focusUnderline(app, tx + pillW * .5f, RULE_Y - 1.f + dy, pillW,
                               a);
        tx += pillW + 4.f;
    }
    r.rect(L::MARGIN, RULE_Y + dy, L::RIGHT - L::MARGIN, 1.f,
           fade(pal.line, a));

    /* Body: labelled sections of rows. */
    const int focusedRow = currentRow();
    float y = BODY_Y + dy;
    char buf[48];
    for (const Section& sec : TABS[m_tab].sections) {
        ui::label(app, L::MARGIN + bdx,
                  fonts.monoTiny.centerY(y, fonts.monoTiny.capHeight()),
                  sec.label, fade(pal.textMuted, ba));
        y += LABEL_H;
        for (int row : sec.rows) {
            if (row < 0) continue;
            ui::RowStyle style;
            style.focused = row == focusedRow;
            style.adjustable = !isAction(row);
            const bool swatches = row == ROW_ACCENT;
            ui::menuRow(app, L::MARGIN + bdx, y, L::RIGHT - L::MARGIN, L::ROW_H,
                        ROW_LABELS[row],
                        swatches ? nullptr : valueText(app, row, buf, sizeof buf),
                        style, ba);
            if (swatches) {
                /* Accent swatches in place of a value, the active one ringed. */
                const float cy = y + L::ROW_H * .5f;
                for (int s = 0; s < theme::ACCENT_COUNT; s++) {
                    const float cx = L::RIGHT - 16.f + bdx -
                                     float(theme::ACCENT_COUNT - 1 - s) * 16.f;
                    if (s == cfg::get().accent)
                        ui::pixelFrame(r, cx - 6.f, cy - 6.f, 12.f, 12.f, 1, 2,
                                       fade(style.focused ? pal.onAccent
                                                          : pal.textPrimary, ba));
                    ui::pixelRect(r, cx - 4.f, cy - 4.f, 8.f, 8.f, 2,
                                  fade(rsHex(theme::accentOption(s).rgb), ba));
                }
            }
            y += L::ROW_H;
        }
        y += SECTION_GAP;
    }

    /* About lives at the end of System, quiet: testers report against a
     * build, and a mismatched EBOOT/core set fails with an API version
     * error that is otherwise hard to read. */
    if (m_tab == TAB_SYSTEM) {
        ui::label(app, L::MARGIN + bdx,
                  fonts.monoTiny.centerY(y, fonts.monoTiny.capHeight()),
                  "ABOUT", fade(pal.textMuted, ba));
        y += LABEL_H + 2.f;
        char line[96];
        std::snprintf(line, sizeof line, "RETROSHELL %s", RS_RELEASE_VERSION);
        fonts.monoTiny.draw(r, L::MARGIN + 10.f + bdx, y, line,
                            fade(pal.textSecondary, ba));
        std::snprintf(line, sizeof line, "BUILT %s UTC  \xC2\xB7  CORE API V%u",
                      RS_BUILD_STAMP, unsigned(RS_CORE_API_VERSION));
        fonts.monoTiny.draw(r, L::MARGIN + 10.f + bdx, y + 13.f, line,
                            fade(pal.textMuted, ba));
    }

    using B = ui::prim::Button;
    if (m_index < 0) {
        const App::Hint hints[] = {
            {B::DpadLeftRight, "Tab"}, {B::DpadDown, "Enter"},
            {B::Circle, "Back"},
        };
        app.drawHintBar(hints, 3);
    } else {
        App::Hint hints[4];
        int n = 0;
        hints[n++] = {B::L1, ""};
        hints[n++] = {B::R1, "Tab"};
        hints[n++] = isAction(focusedRow) ? App::Hint{B::Cross, "Select"}
                                          : App::Hint{B::DpadLeftRight, "Change"};
        hints[n++] = {B::Circle, "Back"};
        app.drawHintBar(hints, n);
    }
}

}  // namespace rs
