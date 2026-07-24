/** Base interface for UI scenes (boot, home, browser, settings, in-game). */
#pragma once

namespace rs {

class App;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void enter(App&) {}
    virtual void update(App& app, float dt) = 0;
    virtual void draw(App& app) = 0;
};

}  // namespace rs
