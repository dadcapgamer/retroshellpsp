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

/* Boot → home entrance → navigate right twice → open pulse → theme toggle
 * to light → navigate to the far left (Recent). */
constexpr Step SCRIPT[] = {
    {30,   0,               "boot"},
    {120,  0,               "home_dark"},
    {140,  PSP_CTRL_RIGHT,  nullptr},
    {160,  PSP_CTRL_RIGHT,  nullptr},
    {200,  0,               "home_nav"},
    {210,  PSP_CTRL_CROSS,  nullptr},
    {220,  0,               "home_pulse"},
    {240,  PSP_CTRL_SQUARE, nullptr},
    {300,  0,               "home_light"},
    {310,  PSP_CTRL_LEFT,   nullptr},
    {318,  PSP_CTRL_LEFT,   nullptr},
    {326,  PSP_CTRL_LEFT,   nullptr},
    {334,  PSP_CTRL_LEFT,   nullptr},
    {380,  0,               "home_first"},
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
