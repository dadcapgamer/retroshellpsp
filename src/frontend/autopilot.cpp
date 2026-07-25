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

/* Phase 4 flow: boot → GBC list (Cave Dave, a real ROM) → Cross triggers
 * the adaptive core picker (dummy + gambatte both claim gbc) → pick
 * gambatte → real emulation → save/load a state → exit → relaunch, which
 * must now SKIP the picker (core remembered per game) → exit → the
 * detail panel reads "via gambatte". */
constexpr Step SCRIPT[] = {
    {30,   0,                 "boot"},
    {150,  0,                 "home"},
    {165,  PSP_CTRL_RIGHT,    nullptr},   /* GB → GBC */
    {173,  PSP_CTRL_RIGHT,    nullptr},   /* → GBA */
    {181,  PSP_CTRL_RIGHT,    nullptr},   /* → NES */
    {189,  PSP_CTRL_RIGHT,    nullptr},   /* → SNES */
    {205,  PSP_CTRL_DOWN,     nullptr},   /* into the SNES game list */
    {240,  0,                 "list_snes"},
    {260,  PSP_CTRL_CROSS,    nullptr},   /* picker: dummy + snes9x2005 */
    {345,  PSP_CTRL_DOWN,     nullptr},   /* → snes9x2005 */
    {370,  0,                 "core_picker_snes"},
    {390,  PSP_CTRL_CROSS,    nullptr},   /* run with snes9x2005 */
    {700,  0,                 "game_title"},
    {720,  PSP_CTRL_START,    nullptr},   /* past the title screen */
    {721,  PSP_CTRL_START,    nullptr},
    {722,  PSP_CTRL_START,    nullptr},
    {950,  0,                 "game_playing"},
    {920,  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, nullptr},
    {921,  PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START, nullptr},
    {990,  0,                 "game_menu"},
    {1005, PSP_CTRL_DOWN,     nullptr},   /* → Save state */
    {1020, PSP_CTRL_CROSS,    nullptr},   /* save slot 1 */
    {1100, 0,                 "menu_saved"},
    {1115, PSP_CTRL_DOWN,     nullptr},   /* → Load state */
    {1130, PSP_CTRL_CROSS,    nullptr},   /* load slot 1, resumes */
    {1200, 0,                 "game_loaded"},
    {1220, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, nullptr},
    {1221, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START, nullptr},
    {1250, PSP_CTRL_DOWN,     nullptr},
    {1258, PSP_CTRL_DOWN,     nullptr},
    {1266, PSP_CTRL_DOWN,     nullptr},
    {1274, PSP_CTRL_DOWN,     nullptr},
    {1282, PSP_CTRL_DOWN,     nullptr},   /* → Exit game */
    {1300, PSP_CTRL_CROSS,    nullptr},
    {1400, 0,                 "home_back"},
    /* Relaunch: same game, no picker this time — gambatte is remembered. */
    {1420, PSP_CTRL_CROSS,    nullptr},
    {1560, 0,                 "game_direct_relaunch"},
    {1580, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER, nullptr},
    {1581, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_START, nullptr},
    {1610, PSP_CTRL_DOWN,     nullptr},
    {1618, PSP_CTRL_DOWN,     nullptr},
    {1626, PSP_CTRL_DOWN,     nullptr},
    {1634, PSP_CTRL_DOWN,     nullptr},
    {1642, PSP_CTRL_DOWN,     nullptr},
    {1660, PSP_CTRL_CROSS,    nullptr},
    {1770, 0,                 "detail_via_core"},
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
