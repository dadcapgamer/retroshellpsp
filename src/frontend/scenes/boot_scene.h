/** Boot splash: the theme's splash, held, then hands off to Home. */
#pragma once

#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"
#include "platform/psp/gu_renderer.h"

namespace rs {

class BootScene : public Scene {
public:
    ~BootScene() override;
    void enter(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    gfx::Texture m_logo;
    float m_t = 0.f;
    bool  m_handedOff = false;
};

}  // namespace rs
