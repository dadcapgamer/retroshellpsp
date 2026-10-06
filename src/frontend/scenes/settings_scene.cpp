#include "frontend/scenes/settings_scene.h"

#include "core_api/rs_core_api.h"

#include "rs_build_stamp.h"
#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "frontend/scenes/setup_scene.h"
#include "platform/psp/power.h"
#include "runtime/config.h"

#include <pspctrl.h>

#include <cstdio>
#include <cstring>

namespace rs {

namespace {
const char* ROW_LABELS[] = {
    "Theme", "Accent color", "Time format", "Artwork", "Menu CPU clock",
    "In-game CPU clock", "UI sounds", "Show FPS", "Auto-save",
    "Rescan library", "Run setup again",
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
    m_entrance.start(0.22f);
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

void SettingsScene::activate(App& app) {
    if (m_row == ROW_SETUP) {
        app.switchScene(std::make_unique<SetupScene>(/*fromSettings=*/true));
    } else if (m_row == ROW_RESCAN) {
        if (!app.scanner().running()) {
            app.scanner().start();
            app.toast("Rescanning library...");
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
    if (pad.navPressed(PSP_CTRL_UP) && m_row > 0) m_row--;
    if (pad.navPressed(PSP_CTRL_DOWN) && m_row < ROW_COUNT - 1) m_row++;
    if (pad.navPressed(PSP_CTRL_LEFT)) adjust(app, -1);
    if (pad.navPressed(PSP_CTRL_RIGHT)) adjust(app, 1);
    if (pad.isPressed(PSP_CTRL_CROSS)) activate(app);
    /* O goes back; Start jumps Home like everywhere else. */
    if (pad.isPressed(PSP_CTRL_CIRCLE) || pad.isPressed(PSP_CTRL_START) ||
        pad.isPressed(PSP_CTRL_TRIANGLE))
        app.switchScene(std::make_unique<HomeScene>());

    m_entrance.update(dt);
}

void SettingsScene::draw(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();

    app.drawBackground();
    app.drawTopBar(false);

    /* Settings arrives from below, like the Library: a short vertical shift
     * and fade, reversed by the scene fade on the way back. */
    const float enter = ui::easeOutCubic(m_entrance.t);
    const u32 a = u32(enter * 255.f);
    const float dy = (1.f - enter) * 24.f;
    auto fade = [&](u32 c) {
        return rsWithAlpha(c, rsAlphaOf(c) * a / 255u);
    };

    fonts.pixelMedium.drawBold(r, 21.f, 13.f + dy, "SETTINGS",
                               fade(pal.textPrimary));
    /* Build identity. Testers report problems against a build, and matching
     * a PSP to the artifact that produced it was previously only possible by
     * reading the log. The core API version is included because a mismatched
     * EBOOT and core set fails with a version error that is otherwise hard
     * to interpret. */
    char build[96];
    std::snprintf(build, sizeof build, "%s \xC2\xB7 built %s UTC \xC2\xB7 core API v%u",
                  RS_RELEASE_VERSION, RS_BUILD_STAMP,
                  unsigned(RS_CORE_API_VERSION));
    fonts.pixelTiny.draw(r, 21.f, 34.f + dy, build, fade(pal.textSecondary));

    constexpr float TOP = 48.f, ROW = 17.f, X = 12.f, W = 456.f;
    char buf[48];
    for (int i = 0; i < ROW_COUNT; i++) {
        const float y = TOP + float(i) * ROW + dy;
        const bool sel = i == m_row;
        const float textY = y + (ROW - fonts.pixelSmall.lineHeight()) * .5f;
        if (sel) {
            r.rect(X, y, W, ROW, fade(pal.selectBg));
            ui::prim::chevron(r, ui::prim::Dir::Right, 24.f, y + ROW * .5f,
                              5.f, 2.f, fade(pal.selectText));
        }
        const u32 ink = sel ? pal.selectText : pal.textPrimary;
        fonts.pixelSmall.draw(r, 36.f, textY, ROW_LABELS[i], fade(ink));
        if (i == ROW_ACCENT) {
            /* The accent row shows its swatches in place of a value. */
            for (int s = 0; s < theme::ACCENT_COUNT; s++) {
                const float cx = X + W - 14.f -
                                 float(theme::ACCENT_COUNT - 1 - s) * 14.f;
                const float cy = y + ROW * .5f;
                if (s == cfg::get().accent)
                    ui::prim::ring(r, cx, cy, 6.f, fade(ink));
                ui::prim::circle(r, cx, cy, 4.f,
                                 fade(rsHex(theme::accentOption(s).rgb)));
            }
        } else {
            fonts.pixelSmall.draw(r, X + W - 12.f, textY,
                                  valueText(app, i, buf, sizeof buf),
                                  fade(ink), text::Align::Right);
        }
    }

    const App::Hint hints[] = {
        {ui::prim::Button::Cross, "Change"},
        {ui::prim::Button::Circle, "Back"},
        {ui::prim::Button::Start, "Home"},
    };
    app.drawHintBar(hints, 3);
}

}  // namespace rs
