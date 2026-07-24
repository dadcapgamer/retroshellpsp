/** Application shell: main loop, scene stack, theme state, and the shared
 * chrome (animated background, top bar, hint bar) every scene draws through.
 */
#pragma once

#include "frontend/scenes/scene.h"
#include "frontend/themes/palette.h"
#include "frontend/text/font.h"
#include "frontend/ui/anim.h"
#include "frontend/ui/prim.h"
#include "platform/psp/gu_renderer.h"
#include "platform/psp/input_pad.h"
#include "rs_common.h"

#include <memory>

namespace rs {

class App {
public:
    bool init();
    void shutdown();
    void run();

    /* --- services ------------------------------------------------------ */
    gfx::Renderer&    renderer()       { return m_renderer; }
    const input::Pad& pad() const      { return m_pad; }
    input::Pad&       padMutable()     { return m_pad; }

    struct Fonts {
        text::Font title;   /* Inter SemiBold 26 */
        text::Font large;   /* Inter SemiBold 19 */
        text::Font body;    /* Inter Regular 15  */
        text::Font small;   /* Inter Regular 12  */
    };
    const Fonts& fonts() const { return m_fonts; }

    /* --- theme ---------------------------------------------------------- */
    const theme::Palette& pal() const { return m_pal; }
    bool darkTheme() const            { return m_darkTheme; }
    void setTheme(bool dark);         /* animated crossfade */
    void toggleTheme()                { setTheme(!m_darkTheme); }

    /* --- scenes ---------------------------------------------------------- */
    /* Takes ownership; transitions through a brief scrim fade. */
    void switchScene(std::unique_ptr<Scene> next, bool instant = false);

    /* --- shared chrome ---------------------------------------------------- */
    void drawBackground();
    void drawTopBar();
    struct Hint { ui::prim::Button button; const char* label; };
    void drawHintBar(const Hint* hints, int count);

    float time() const { return m_time; }

private:
    void update(float dt);
    void draw();
    void drawWave(float baseY, float amp, float freq, float speed,
                  float phase, float height, u32 color);
#ifdef RS_DEBUG_OVERLAY
    void drawDebugOverlay();
#endif

    gfx::Renderer m_renderer;
    input::Pad    m_pad;
    Fonts         m_fonts;

    std::unique_ptr<Scene> m_scene;
    std::unique_ptr<Scene> m_pending;
    ui::Tween m_sceneFade;          /* 0..1: out then in */
    bool m_fadingOut = false;

    theme::Palette m_pal = theme::dark();
    bool m_darkTheme = true;
    ui::Tween m_themeFade;
    theme::Palette m_themeFrom = theme::dark();

    float m_time = 0.f;
    u32   m_lastUs = 0;

    int  m_batteryPct   = -1;
    bool m_batteryChg   = false;
    int  m_batteryPoll  = 0;

#ifdef RS_DEBUG_OVERLAY
    bool m_showOverlay = false;
#endif
};

}  // namespace rs
