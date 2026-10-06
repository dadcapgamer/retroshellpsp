#include "frontend/scenes/setup_scene.h"

#include "frontend/app.h"
#include "frontend/scenes/home_scene.h"
#include "frontend/scenes/settings_scene.h"
#include "frontend/ui/state_panel.h"
#include "frontend/ui/text_layout.h"
#include "runtime/config.h"

#include <pspctrl.h>

#include <cstdio>

namespace rs {

namespace {
constexpr float MIN_SCAN_SECONDS = 0.7f;   /* never flash past the first step */

u32 fade(u32 color, u32 alpha) {
    return rsWithAlpha(color, rsAlphaOf(color) * alpha / 255u);
}
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
    constexpr float TOP = 54.f, ROW = 19.f, X = 12.f, W = 456.f;
    const int n = int(m_rows.size());
    for (int i = 0; i < n; i++) {
        const SystemRow& row = m_rows[size_t(i)];
        const float y = TOP + float(i) * ROW;
        const bool sel = emulators && i == m_row;
        const float textY = y + (ROW - fonts.pixelSmall.lineHeight()) * .5f;
        if (sel) {
            r.rect(X, y, W, ROW, fade(pal.selectBg, a));
            ui::prim::chevron(r, ui::prim::Dir::Right, 24.f, y + ROW * .5f, 5.f,
                              2.f, fade(pal.selectText, a));
        }
        const u32 ink = sel ? pal.selectText : pal.textPrimary;
        const u32 sub = sel ? pal.selectText : pal.textSecondary;
        fonts.pixelSmall.draw(r, 36.f, textY,
                              db::systemInfo(db::System(row.system)).displayName,
                              fade(ink, a));
        if (!emulators) {
            char count[24];
            std::snprintf(count, sizeof count, "%d game%s", row.games,
                          row.games == 1 ? "" : "s");
            fonts.pixelSmall.draw(r, X + W - 12.f, textY, count, fade(sub, a),
                                  text::Align::Right);
            continue;
        }
        if (row.cores.empty()) {
            fonts.pixelSmall.draw(r, X + W - 12.f, textY, "No emulator installed",
                                  fade(sel ? pal.selectText : rsHex(0xE05252), a),
                                  text::Align::Right);
            continue;
        }
        const std::string label = row.cores[size_t(row.choice)]->name;
        float right = X + W - 12.f;
        if (row.cores.size() > 1) {
            ui::prim::chevron(r, ui::prim::Dir::Right, right - 3.f,
                              y + ROW * .5f, 4.f, 2.f, fade(ink, a));
            right -= 14.f;
        }
        fonts.pixelSmall.draw(r, right, textY, label.c_str(), fade(ink, a),
                              text::Align::Right);
        if (row.cores.size() > 1)
            ui::prim::chevron(r, ui::prim::Dir::Left,
                              right - fonts.pixelSmall.measure(label.c_str()) -
                                  10.f, y + ROW * .5f, 4.f, 2.f, fade(ink, a));
    }
}

void SetupScene::draw(App& app) {
    const auto& pal = app.pal();
    const auto& fonts = app.fonts();
    app.drawBackground();
    app.drawTopBar(false);

    const float enter = ui::easeOutCubic(m_entrance.t);
    const u32 a = u32(enter * 255.f);
    using B = ui::prim::Button;

    const char* titles[] = {"SETUP", "YOUR SYSTEMS", "EMULATORS", "ALL SET"};
    fonts.pixelMedium.drawBold(app.renderer(), 21.f, 13.f,
                               titles[int(m_step)], fade(pal.textPrimary, a));

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
            const App::Hint hints[] = {{B::Start, "Skip setup"}};
            app.drawHintBar(hints, 1);
            break;
        }
        case Step::Systems: {
            if (m_rows.empty()) {
                ui::StatePanel panel;
                panel.kind = ui::StatePanel::Kind::Empty;
                panel.title = "No games found";
                panel.message = "Copy ROMs into ms0:/ROMS (sub-folders are "
                                "fine), then scan again.";
                panel.actionLabel = "Scan again";
                panel.actionButton = B::Cross;
                ui::drawStatePanel(app, panel, a);
                const App::Hint hints[] = {{B::Cross, "Scan again"},
                                           {B::Triangle, "Finish anyway"}};
                app.drawHintBar(hints, 2);
                break;
            }
            char summary[64];
            std::snprintf(summary, sizeof summary, "%d games in %d system%s",
                          m_totalGames, int(m_rows.size()),
                          m_rows.size() == 1 ? "" : "s");
            fonts.pixelTiny.draw(app.renderer(), 459.f, 34.f, summary,
                                 fade(pal.textSecondary, a), text::Align::Right);
            drawRows(app, a, false);
            const App::Hint hints[] = {{B::Cross, "Continue"},
                                       {B::Start, "Skip"}};
            app.drawHintBar(hints, 2);
            break;
        }
        case Step::Emulators: {
            fonts.pixelTiny.draw(app.renderer(), 459.f, 34.f,
                                 "Left / right changes the default emulator",
                                 fade(pal.textSecondary, a),
                                 text::Align::Right);
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
            panel.actionLabel = "Start playing";
            panel.actionButton = B::Cross;
            ui::drawStatePanel(app, panel, a);
            const App::Hint hints[] = {{B::Cross, "Done"}, {B::Circle, "Back"}};
            app.drawHintBar(hints, 2);
            break;
        }
    }
}

}  // namespace rs
