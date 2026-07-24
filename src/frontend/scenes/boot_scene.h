/** Boot splash: wordmark fade-in, then hands off to the home scene. */
#pragma once

#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"

namespace rs {

class BootScene : public Scene {
public:
    void enter(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    float m_t = 0.f;
    bool  m_handedOff = false;
};

}  // namespace rs
