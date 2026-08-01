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
    "Theme", "Accent color", "Time format", "Menu CPU clock",
    "In-game CPU clock", "UI sounds", "Show FPS", "Auto-save",
    "PSP-1000 Safe Mode", "Rescan library",
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
    m_scroll.snap(0.f);
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
        case ROW_PSP1000_SAFE:
            c.psp1000SafeMode = !c.psp1000SafeMode;
            c.psp1000SafeModeConfigured = true;
            cfg::save();
            app.cores().discover();
            app.toast(c.psp1000SafeMode
                          ? "Showing PSP-1000 qualified cores"
                          : "Experimental cores are now visible");
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
        case ROW_ACCENT:
            std::snprintf(buf, n, "%s",
                          theme::accentOption(c.accent).name);
            return buf;
        case ROW_TIME_FORMAT: return c.clock24Hour ? "24-hour" : "12-hour";
        case ROW_CPU_MENU:
            std::snprintf(buf, n, "%d MHz", c.cpuMenuMhz);
            return buf;
        case ROW_CPU_GAME:
            std::snprintf(buf, n, "%d MHz", c.cpuGameMhz);
            return buf;
        case ROW_UI_SOUNDS: return c.uiSounds ? "On" : "Off";
        case ROW_SHOW_FPS:  return c.showFps ? "On" : "Off";
        case ROW_AUTOSAVE:  return c.autosave ? "On" : "Off";
        case ROW_PSP1000_SAFE: return c.psp1000SafeMode ? "On" : "Off";
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
    constexpr int VISIBLE_ROWS = 4;
    m_scroll.to(float(rsClamp(m_row - 2, 0, ROW_COUNT - VISIBLE_ROWS)));
    m_scroll.update(dt, 14.f);
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

    fonts.large.drawShadow(r, 16.f, 32.f + slide, "Settings",
                           rsWithAlpha(pal.textPrimary, a), pal.shadow);

    constexpr int VISIBLE_ROWS = 4;
    const float px = 16.f, pw = 448.f, py = 64.f + slide;
    const float rowH = 40.f;
    const float ph = float(VISIBLE_ROWS) * rowH;

    /* Full-width rows align to the same 16px content rail as the heading.
     * Selection, not an enclosing card, supplies the necessary grouping. */
    const float hy = py + (m_rowPos.v - m_scroll.v) * rowH;
    ui::prim::focusRow(r, px, hy + 4.f, pw, 32.f,
                       rsWithAlpha(pal.tileFocusBg,
                                   rsAlphaOf(pal.tileFocusBg) * a / 255u),
                       rsWithAlpha(pal.accent, a),
                       rsWithAlpha(pal.shadow,
                                   rsAlphaOf(pal.shadow) * a / (255u * 2u)));

    char buf[48];
    r.setScissor(int(px), int(py), int(pw), int(ph));
    for (int i = 0; i < ROW_COUNT; i++) {
        const float rowY = py + (float(i) - m_scroll.v) * rowH;
        if (rowY < py || rowY >= py + ph) continue;
        /* Font::draw receives the top of its 15px line box. Centre that
         * box in the 40px logical row rather than aligning it to the
         * 32px focus shape's top edge. */
        const float y =
            rowY + float(int((rowH - fonts.body.lineHeight()) * .5f));
        const bool sel = i == m_row;
        fonts.body.draw(r, px + 16.f, y, ROW_LABELS[i],
                        rsWithAlpha(sel ? pal.textPrimary : pal.textSecondary,
                                    a));
        if (!(i == ROW_ACCENT && sel))
            fonts.body.draw(r, px + pw - 16.f, y,
                            valueText(app, i, buf, sizeof buf),
                            rsWithAlpha(sel ? pal.textPrimary : pal.textDim, a),
                            text::Align::Right);
        if (i < ROW_COUNT - 1)
            r.rect(px + 16.f, rowY + 39.f, pw - 32.f, 1.f,
                   rsWithAlpha(pal.panelOutline,
                               rsAlphaOf(pal.panelOutline) * a / 255u));
        /* The expanded palette replaces, rather than overlays, the normal
         * one-color value indicator while this row is selected. */
        if (i == ROW_ACCENT && !sel) {
            ui::prim::circle(
                r, px + pw - 104.f, rowY + rowH * .5f, 4.f,
                rsWithAlpha(
                    rsHex(theme::accentOption(cfg::get().accent).rgb), a));
        }
    }
    if (m_row == ROW_ACCENT) {
        /* Palette contents belong to the selected data row, not to the
         * animated focus outline. Anchoring to the row prevents the dots
         * floating above their label for several frames after navigation. */
        const float selectedRowY =
            py + (float(m_row) - m_scroll.v) * rowH;
        for (int i = 0; i < theme::ACCENT_COUNT; i++) {
            const float cx =
                px + pw - 32.f -
                float(theme::ACCENT_COUNT - 1 - i) * 16.f;
            const float cy = selectedRowY + rowH * .5f;
            if (i == cfg::get().accent)
                ui::prim::ring(r, cx, cy, 6.f,
                               rsWithAlpha(pal.textPrimary, a));
            ui::prim::circle(
                r, cx, cy, 4.f,
                rsWithAlpha(rsHex(theme::accentOption(i).rgb), a));
        }
    }
    r.resetScissor();

    const App::Hint hints[] = {
        {ui::prim::Button::Cross, "Change"},
        {ui::prim::Button::Circle, "Back"},
    };
    app.drawHintBar(hints, 2);
}

}  // namespace rs
