/** First-run setup: scan, confirm systems, confirm emulators, done.
 *
 * Kept short on purpose — under a minute, and every step that has nothing to
 * decide is skipped: a library whose systems each have exactly one emulator
 * goes Scan -> Systems -> Done. Nothing here is required to use the shell;
 * Start skips straight to Done, and Settings can run it again.
 */
#pragma once

#include "frontend/core_registry.h"
#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"
#include "rs_common.h"

#include <vector>

namespace rs {

class SetupScene : public Scene {
public:
    explicit SetupScene(bool fromSettings = false)
        : m_fromSettings(fromSettings) {}

    void enter(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    enum class Step : u8 { Scan, Systems, Emulators, Done };

    struct SystemRow {
        int system = 0;                    /* db::System id */
        int games = 0;
        std::vector<const CoreInfo*> cores;
        int choice = 0;                    /* index into cores */
    };

    void buildRows(App& app);
    bool needsEmulatorStep() const;
    void advance(App& app);
    void retreat();
    void finish(App& app);
    void applyChoices(App& app);
    void drawRows(App& app, u32 alpha, bool emulators);

    Step  m_step = Step::Scan;
    bool  m_fromSettings = false;
    float m_stepTime = 0.f;
    int   m_idleFrames = 0;
    bool  m_scanStarted = false;
    int   m_row = 0;
    int   m_totalGames = 0;
    std::vector<SystemRow> m_rows;
    ui::Tween m_entrance;
};

}  // namespace rs
