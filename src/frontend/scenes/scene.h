/** Base interface for UI scenes (boot, home, browser, settings, in-game). */
#pragma once

namespace rs {

class App;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void enter(App&) {}
    /* Called while App services are still alive during application exit.
     * Most scenes own no external resources, but an active GameSession must
     * flush saves and unload its PRX before the arena and logger disappear. */
    virtual void shutdown(App&) {}
    /* Power callbacks are delivered on a PSP callback thread, then forwarded
     * here on the main thread after wake. Scenes can safely quiesce and
     * restore hardware-facing state without touching it from the callback. */
    virtual void systemSuspend(App&) {}
    virtual void systemResume(App&) {}
    virtual void update(App& app, float dt) = 0;
    virtual void draw(App& app) = 0;
};

}  // namespace rs
