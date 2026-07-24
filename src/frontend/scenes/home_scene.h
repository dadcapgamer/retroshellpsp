/** Home: XMB-inspired horizontal category bar (systems + Recent, Favorites,
 * Settings) over the animated background. Phase 2 fills the content area
 * with the scanned game library.
 */
#pragma once

#include "frontend/scenes/scene.h"
#include "frontend/ui/anim.h"
#include "rs_common.h"

namespace rs {

class HomeScene : public Scene {
public:
    void enter(App& app) override;
    void update(App& app, float dt) override;
    void draw(App& app) override;

private:
    int        m_catIdx = 2;      /* start on the first system */
    ui::Smooth m_catPos;
    ui::Tween  m_entrance;
    ui::Tween  m_pulse;           /* focused-tile pop on select */
};

}  // namespace rs
