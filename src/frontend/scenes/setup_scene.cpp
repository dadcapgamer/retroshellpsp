#include "frontend/scenes/setup_scene.h"

#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "frontend/scenes/settings_scene.h"
#include "frontend/ui/chrome.h"
#include "frontend/ui/state_panel.h"
#include "frontend/ui/text_layout.h"
#include "runtime/config.h"

#include <pspctrl.h>

#include <cstdio>

namespace rs {

namespace {
constexpr float MIN_SCAN_SECONDS = 0.7f;   /* never flash past the first step */

namespace L = ui::layout;
using ui::fade;

/* Step heading under the shell header: progress overline, then a title. */
constexpr float HEAD_Y = 36.f;
constexpr float ROWS_Y = 72.f;
}  // namespace

void SetupScene::enter(App& app) {
    m_step = Step::Scan;
    m_stepTime = 0.f;
    m_idleFrames = 0;
    m_scanStarted = false;
    m_row = 0;
    m_entrance.start(0.25f);
    /* The boot path may already have a scan running (no cache yet); a re-run
     * from Settings starts a fresh one so the numbers are current. */
    if (!app.scanner().running()) {
        app.scanner().start();
        m_scanStarted = true;
    }
}

void SetupScene::buildRows(App& app) {
    m_rows.clear();
    m_totalGames = 0;
    for (int s = 0; s < db::SYSTEM_COUNT; s++) {
        const auto& games = app.index().games(db::System(s));
        if (games.empty()) continue;
        SystemRow row;
        row.system = s;
        row.games = int(games.size());
        row.cores = app.cores().coresFor(db::System(s));
        const CoreInfo* current = app.cores().defaultFor(db::System(s));
        for (size_t i = 0; i < row.cores.size(); i++)
            if (row.cores[i] == current) row.choice = int(i);
        m_totalGames += row.games;
        m_rows.push_back(std::move(row));
    }
}

/* A step with nothing to decide is not shown: only systems with a real
 * choice, or with no emulator at all (which the user should hear about
 * before hitting a launch error), warrant the screen. */
bool SetupScene::needsEmulatorStep() const {
    for (const auto& row : m_rows)
        if (row.cores.size() != 1) return true;
    return false;
}

void SetupScene::applyChoices(App& app) {
    for (const auto& row : m_rows) {
        if (row.cores.size() < 2) continue;
        const char* coreId = db::systemInfo(db::System(row.system)).coreId;
        /* The top-priority core is the automatic default; only a deliberate
         * departure from it is stored, so future core installs still sort
         * naturally for everyone who accepted the default. */
        const CoreInfo* chosen = row.cores[size_t(row.choice)];
        cfg::setSystemCore(coreId, row.choice == 0 ? "" : chosen->name.c_str());
    }
    (void)app;
}

void SetupScene::advance(App& app) {
    m_stepTime = 0.f;
    m_row = 0;
    switch (m_step) {
        case Step::Scan:
            buildRows(app);
            m_step = Step::Systems;
            break;
        case Step::Systems:
            m_step = needsEmulatorStep() ? Step::Emulators : Step::Done;
            break;
        case Step::Emulators:
            applyChoices(app);
            m_step = Step::Done;
            break;
        case Step::Done:
            finish(app);
            break;
    }
}

void SetupScene::retreat() {
    m_stepTime = 0.f;
    m_row = 0;
    if (m_step == Step::Done) m_step = needsEmulatorStep() ? Step::Emulators
                                                           : Step::Systems;
    else if (m_step == Step::Emulators) m_step = Step::Systems;
}

void SetupScene::finish(App& app) {
    applyChoices(app);
    cfg::get().setupDone = true;
    cfg::save();
    if (m_fromSettings) app.switchScene(std::make_unique<SettingsScene>());
    else app.switchScene(std::make_unique<HomeScene>());
}

void SetupScene::update(App& app, float dt) {
    m_entrance.update(dt);
    m_stepTime += dt;
    const auto& pad = app.pad();

    /* Start skips whatever is left; the shell works without any of this. */
    if (pad.isPressed(PSP_CTRL_START) && m_step != Step::Done) {
        if (m_step == Step::Scan) buildRows(app);
        else if (m_step == Step::Emulators) applyChoices(app);
        finish(app);
        return;
    }

    switch (m_step) {
        case Step::Scan: {
            /* The scanner finishes on its own thread and App::update folds
             * the result in on the next frame, so wait two idle frames. */
            if (!app.scanner().running()) m_idleFrames++;
            else m_idleFrames = 0;
            if (m_idleFrames >= 2 && m_stepTime >= MIN_SCAN_SECONDS)
                advance(app);
            break;
        }
        case Step::Systems:
            if (m_rows.empty()) {
                if (pad.isPressed(PSP_CTRL_CROSS)) {
                    app.scanner().start();
                    m_step = Step::Scan;
                    m_stepTime = 0.f;
                    m_idleFrames = 0;
                } else if (pad.isPressed(PSP_CTRL_TRIANGLE)) {
                    finish(app);
                }
            } else if (pad.isPressed(PSP_CTRL_CROSS)) {
                advance(app);
            }
            break;
        case Step::Emulators: {
            const int n = int(m_rows.size());
            if (pad.navPressed(PSP_CTRL_UP) && m_row > 0) m_row--;
            if (pad.navPressed(PSP_CTRL_DOWN) && m_row < n - 1) m_row++;
            SystemRow& row = m_rows[size_t(rsClamp(m_row, 0, n - 1))];
            const int count = int(row.cores.size());
            if (count > 1) {
                if (pad.navPressed(PSP_CTRL_LEFT))
                    row.choice = (row.choice + count - 1) % count;
                if (pad.navPressed(PSP_CTRL_RIGHT))
                    row.choice = (row.choice + 1) % count;
            }
            if (pad.isPressed(PSP_CTRL_CROSS)) advance(app);
            if (pad.isPressed(PSP_CTRL_CIRCLE)) retreat();
            break;
        }
        case Step::Done:
            if (pad.isPressed(PSP_CTRL_CROSS)) advance(app);
            if (pad.isPressed(PSP_CTRL_CIRCLE)) retreat();
            break;
    }
    if (m_step == Step::Systems && pad.isPressed(PSP_CTRL_CIRCLE) &&
        m_fromSettings)
        finish(app);
}

void SetupScene::drawRows(App& app, u32 a, bool emulators) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    const int n = int(m_rows.size());
    for (int i = 0; i < n; i++) {
        const SystemRow& row = m_rows[size_t(i)];
        const float y = ROWS_Y + float(i) * L::ROW_H;
        const bool sel = emulators && i == m_row;
        char value[48];
        const char* text = value;
        bool missing = false;
        if (!emulators) {
            std::snprintf(value, sizeof value, "%d game%s", row.games,
                          row.games == 1 ? "" : "s");
        } else if (row.cores.empty()) {
            text = "No emulator installed";
            missing = true;
        } else {
            std::snprintf(value, sizeof value, "%s",
                          row.cores[size_t(row.choice)]->name.c_str());
        }
        ui::RowStyle style;
        style.focused = sel;
        style.adjustable = emulators && row.cores.size() > 1;
        ui::menuRow(app, L::MARGIN, y, L::RIGHT - L::MARGIN, L::ROW_H,
                    ui::systemName(row.system),
                    missing ? nullptr : text, style, a);
        if (missing)
            fonts.body.draw(r, L::RIGHT - 10.f, fonts.body.centerY(y, L::ROW_H),
                            text, fade(sel ? pal.onAccent : pal.danger, a),
                            text::Align::Right);
    }
}

void SetupScene::draw(App& app) {
    auto& r = app.renderer();
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    app.drawBackground();
    app.drawTopBar();

    const float enter = ui::easeOutCubic(m_entrance.t);
    const u32 a = u32(enter * 255.f);
    using B = ui::prim::Button;

    /* Steps that list rows get a heading; the others are centred states. */
    auto heading = [&](int step, const char* title, const char* hint) {
        char over[24];
        std::snprintf(over, sizeof over, "STEP %d", step);
        ui::label(app, L::MARGIN, fonts.tiny.centerY(HEAD_Y, 6.f), over,
                  fade(pal.textMuted, a));
        fonts.title.draw(r, L::MARGIN,
                         fonts.title.centerY(HEAD_Y + 14.f,
                                             fonts.title.capHeight()),
                         title, fade(pal.textPrimary, a));
        if (hint)
            fonts.small.draw(r, L::RIGHT,
                             fonts.small.centerY(HEAD_Y + 14.f,
                                                 fonts.title.capHeight()),
                             hint, fade(pal.textMuted, a), text::Align::Right);
        r.rect(L::MARGIN, ROWS_Y - 8.f, L::RIGHT - L::MARGIN, 1.f,
               fade(pal.line, a));
    };

    switch (m_step) {
        case Step::Scan: {
            ui::StatePanel panel;
            panel.kind = ui::StatePanel::Kind::Loading;
            panel.title = "Looking for games";
            panel.message = "Scanning ms0:/ROMS. You only need to do this "
                            "once; RetroShell remembers your library.";
            char progress[40];
            std::snprintf(progress, sizeof progress, "%d files checked",
                          app.scanner().progress());
            panel.detail = progress;
            ui::drawStatePanel(app, panel, a);
            const App::Hint hints[] = {{B::Start, "Skip Setup"}};
            app.drawHintBar(hints, 1);
            break;
        }
        case Step::Systems: {
            if (m_rows.empty()) {
                ui::StatePanel panel;
                panel.kind = ui::StatePanel::Kind::Empty;
                panel.title = "No games found";
                panel.message = "Copy ROMs into ms0:/ROMS (sub-folders are "
                                "fine), then rescan.";
                panel.actionLabel = "Rescan Library";
                panel.actionButton = B::Cross;
                ui::drawStatePanel(app, panel, a);
                const App::Hint hints[] = {{B::Cross, "Rescan Library"},
                                           {B::Triangle, "Finish Anyway"}};
                app.drawHintBar(hints, 2);
                break;
            }
            char summary[64];
            std::snprintf(summary, sizeof summary, "%d games in %d system%s",
                          m_totalGames, int(m_rows.size()),
                          m_rows.size() == 1 ? "" : "s");
            heading(1, "Your systems", summary);
            drawRows(app, a, false);
            const App::Hint hints[] = {{B::Cross, "Continue"},
                                       {B::Start, "Skip"}};
            app.drawHintBar(hints, 2);
            break;
        }
        case Step::Emulators: {
            heading(2, "Default emulators", "Left / right to change");
            drawRows(app, a, true);
            const App::Hint hints[] = {{B::DpadLeftRight, "Change"},
                                       {B::Cross, "Continue"},
                                       {B::Circle, "Back"}};
            app.drawHintBar(hints, 3);
            break;
        }
        case Step::Done: {
            ui::StatePanel panel;
            panel.kind = ui::StatePanel::Kind::Done;
            panel.title = "You're all set";
            char msg[96];
            std::snprintf(msg, sizeof msg,
                          "%d game%s ready. Press Start any time to return "
                          "to the home screen.", m_totalGames,
                          m_totalGames == 1 ? "" : "s");
            panel.message = msg;
            panel.actionLabel = "Start Playing";
            panel.actionButton = B::Cross;
            ui::drawStatePanel(app, panel, a);
            const App::Hint hints[] = {{B::Cross, "Done"}, {B::Circle, "Back"}};
            app.drawHintBar(hints, 2);
            break;
        }
    }
}

}  // namespace rs
