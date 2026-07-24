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

/* Boot → GB list → launch first game through the dummy core (from a ZIP)
 * → verify video/audio/input → in-game menu → save then load a state →
 * exit back to the browser → confirm Recently Played picked it up. */
constexpr Step SCRIPT[] = {
    {30,   0,                 "boot"},
    {150,  0,                 "home"},
    {170,  PSP_CTRL_DOWN,     nullptr},   /* enter GB game list */
    {230,  0,                 "list_gb"},
    {250,  PSP_CTRL_CROSS,    nullptr},   /* launch! */
    {400,  0,                 "game_running"},
    {420,  PSP_CTRL_CROSS,    nullptr},   /* held button lights indicator */
    {421,  PSP_CTRL_CROSS,    nullptr},
    {422,  PSP_CTRL_CROSS,    "game_input"},
    {460,  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, nullptr},
    {461,  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START, nullptr},
    {530,  0,                 "game_menu"},
    {545,  PSP_CTRL_DOWN,     nullptr},   /* → Save state */
    {560,  PSP_CTRL_CROSS,    nullptr},   /* save slot 1 */
    {640,  0,                 "menu_saved"},
    {655,  PSP_CTRL_DOWN,     nullptr},   /* → Load state */
    {670,  PSP_CTRL_CROSS,    nullptr},   /* load slot 1, resumes */
    {740,  0,                 "game_loaded"},
    {760,  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, nullptr},
    {761,  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START, nullptr},
    {790,  PSP_CTRL_DOWN,     nullptr},
    {798,  PSP_CTRL_DOWN,     nullptr},
    {806,  PSP_CTRL_DOWN,     nullptr},
    {814,  PSP_CTRL_DOWN,     nullptr},
    {822,  PSP_CTRL_DOWN,     nullptr},   /* → Exit game */
    {840,  PSP_CTRL_CROSS,    nullptr},
    {940,  0,                 "home_back"},
    {960,  PSP_CTRL_CIRCLE,   nullptr},   /* leave list */
    {975,  PSP_CTRL_LEFT,     nullptr},   /* GB → Favorites → Recent */
    {983,  PSP_CTRL_LEFT,     nullptr},
    {1000, PSP_CTRL_DOWN,     nullptr},   /* into Recent list */
    {1060, 0,                 "recent_list"},
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
