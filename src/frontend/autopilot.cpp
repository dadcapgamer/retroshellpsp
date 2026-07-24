#include "frontend/autopilot.h"

#ifdef RS_AUTOPILOT

#include "frontend/app.h"
#include "runtime/log.h"

#include <pspctrl.h>
#include <pspiofilemgr.h>

#include <cstdio>

namespace rs::autopilot {

namespace {

struct Step {
    int frame;
    u32 press;             /* buttons held for this single frame */
    const char* capture;   /* shot name, or nullptr */
};

/* Boot → GB list (boxart, metadata, zip entry) → favorite → back →
 * Settings (theme cycle incl. a custom Memory Stick theme) → home. */
constexpr Step SCRIPT[] = {
    {30,   0,                 "boot"},
    {150,  0,                 "home"},
    {170,  PSP_CTRL_DOWN,     nullptr},   /* enter GB game list */
    {230,  0,                 "list_gb"},
    {245,  PSP_CTRL_DOWN,     nullptr},
    {260,  PSP_CTRL_DOWN,     nullptr},
    {320,  0,                 "list_nav"},
    {335,  PSP_CTRL_TRIANGLE, nullptr},   /* toggle favorite */
    {380,  0,                 "list_fav"},
    {400,  PSP_CTRL_CIRCLE,   nullptr},   /* back to categories */
    {420,  PSP_CTRL_RIGHT,    nullptr},   /* GB → ... → Settings (9x) */
    {428,  PSP_CTRL_RIGHT,    nullptr},
    {436,  PSP_CTRL_RIGHT,    nullptr},
    {444,  PSP_CTRL_RIGHT,    nullptr},
    {452,  PSP_CTRL_RIGHT,    nullptr},
    {460,  PSP_CTRL_RIGHT,    nullptr},
    {468,  PSP_CTRL_RIGHT,    nullptr},
    {476,  PSP_CTRL_RIGHT,    nullptr},
    {484,  PSP_CTRL_RIGHT,    nullptr},
    {540,  0,                 "home_settings"},
    {555,  PSP_CTRL_CROSS,    nullptr},   /* open settings */
    {640,  0,                 "settings"},
    {655,  PSP_CTRL_RIGHT,    nullptr},   /* theme: dark → light */
    {720,  0,                 "settings_light"},
    {735,  PSP_CTRL_RIGHT,    nullptr},   /* light → midnight (custom) */
    {800,  0,                 "settings_midnight"},
    {820,  PSP_CTRL_CIRCLE,   nullptr},   /* back home */
    {880,  0,                 "home_final"},
};
constexpr int STEPS = int(sizeof(SCRIPT) / sizeof(SCRIPT[0]));

int  s_frame = 0;
bool s_dirReady = false;
bool s_done = false;

}  // namespace

void tick(App& app) {
    if (s_done) return;
    if (!s_dirReady) {
        sceIoMkdir("ms0:/RETROSUITE", 0777);
        sceIoMkdir("ms0:/RETROSUITE/shots", 0777);
        s_dirReady = true;
        RS_LOGI("autopilot: engaged");
    }

    for (int i = 0; i < STEPS; i++) {
        if (SCRIPT[i].frame != s_frame) continue;
        if (SCRIPT[i].press) app.padMutable().simulate(SCRIPT[i].press);
        if (SCRIPT[i].capture) {
            char path[128];
            std::snprintf(path, sizeof path, "ms0:/RETROSUITE/shots/%s.png",
                          SCRIPT[i].capture);
            app.renderer().requestCapture(path);
            RS_LOGI("autopilot: capture %s (app time %d ms)",
                    SCRIPT[i].capture, int(app.time() * 1000.f));
        }
    }

    s_frame++;
    if (s_frame > SCRIPT[STEPS - 1].frame + 30) {
        s_done = true;
        RS_LOGI("autopilot: complete");
    }
}

}  // namespace rs::autopilot

#endif  /* RS_AUTOPILOT */
