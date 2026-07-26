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

/* PSP-1000 regression flow: boot → navigate to a target system → launch →
 * capture sustained gameplay → save/load a state → exit through the
 * frontend menu → verify frontend recovery → relaunch the same core.
 * The one-frame gaps are intentional: Pad::poll must observe a release
 * between navigation presses. */
constexpr Step SCRIPT[] = {
    {30,   0,                 "boot"},
    {150,  0,                 "home"},
    {165,  PSP_CTRL_RIGHT,    nullptr},   /* GB → GBC */
    {173,  PSP_CTRL_RIGHT,    nullptr},   /* → GBA */
    {181,  PSP_CTRL_RIGHT,    nullptr},   /* → NES */
    {189,  PSP_CTRL_RIGHT,    nullptr},   /* → SNES */
#ifndef RS_AUTOPILOT_SNES
    {197,  PSP_CTRL_RIGHT,    nullptr},   /* → Genesis/MD */
#endif
    {205,  PSP_CTRL_DOWN,     nullptr},   /* into the target system list */
    {240,  0,                 "list_target"},
    {260,  PSP_CTRL_CROSS,    nullptr},   /* single core → direct launch */
    {560,  0,                 "game_a"},
    {820,  0,                 "game_b"},
    {1080, 0,                 "game_c"},
    {1120, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START,
                                    nullptr},   /* open frontend menu */
    {1130, PSP_CTRL_DOWN,     nullptr},         /* Save state */
    {1140, PSP_CTRL_CROSS,    nullptr},
    {1150, PSP_CTRL_DOWN,     nullptr},         /* Load state */
    {1160, PSP_CTRL_CROSS,    nullptr},
    {1200, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START,
                                    nullptr},
    {1210, PSP_CTRL_DOWN,     nullptr},
    {1214, PSP_CTRL_DOWN,     nullptr},
    {1218, PSP_CTRL_DOWN,     nullptr},
    {1222, PSP_CTRL_DOWN,     nullptr},
    {1226, PSP_CTRL_DOWN,     nullptr},         /* Exit game */
    {1235, PSP_CTRL_CROSS,    nullptr},
    {1280, 0,                 "returned_home"},
    {1300, PSP_CTRL_CROSS,    nullptr},         /* relaunch selected game */
    {1500, 0,                 "game_relaunch"},
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
