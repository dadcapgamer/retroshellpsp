#include "frontend/autopilot.h"

#ifdef RS_AUTOPILOT

#include "frontend/app.h"
#include "platform/psp/fs_psp.h"
#include "runtime/log.h"

#include <pspctrl.h>
#include <pspiofilemgr.h>

#include <cstdio>

extern volatile bool g_exitRequested;

namespace rs::autopilot {

namespace {

struct Step {
    int frame;
    u32 press;             /* buttons held for this single frame */
    const char* capture;   /* shot name, or nullptr */
};

/* PSP-1000 regression flow: boot → navigate to the configured system → launch →
 * capture sustained gameplay → save/load a state → exit through the
 * frontend menu → verify frontend recovery → relaunch the same core.
 * The one-frame gaps are intentional: Pad::poll must observe a release
 * between navigation presses. */
constexpr Step SCRIPT[] = {
    {30,   0,                 "boot"},
    {150,  0,                 "home"},
    {229,  PSP_CTRL_CROSS,    nullptr},   /* open the target system */
    {240,  0,                 "list_target"},
    {248,  PSP_CTRL_TRIANGLE, nullptr},   /* selected-game quick actions */
    {260,  0,                 "quick_actions"},
    {264,  PSP_CTRL_DOWN,     nullptr},   /* Favorite */
    {268,  PSP_CTRL_CROSS,    nullptr},
    {272,  PSP_CTRL_CIRCLE,   nullptr},
    {280,  PSP_CTRL_CROSS,    nullptr},   /* default core → direct launch */
    {560,  0,                 "game_a"},
    {820,  0,                 "game_b"},
    {1080, 0,                 "game_c"},
    {1120, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT,
                                    nullptr},   /* open frontend menu */
    {1126, 0,                 "pause_menu"},
    {1130, PSP_CTRL_DOWN,     nullptr},         /* Save state */
    {1140, PSP_CTRL_CROSS,    nullptr},
    {1150, PSP_CTRL_DOWN,     nullptr},         /* Load state */
    {1160, PSP_CTRL_CROSS,    nullptr},
    {1200, PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT,
                                    nullptr},
    {1210, PSP_CTRL_DOWN,     nullptr},
    {1214, PSP_CTRL_DOWN,     nullptr},
    {1218, PSP_CTRL_DOWN,     nullptr},
    {1222, PSP_CTRL_DOWN,     nullptr},
    {1226, PSP_CTRL_DOWN,     nullptr},
    {1230, PSP_CTRL_DOWN,     nullptr},
    {1232, 0,                 "pause_fast_scroll"},
    {1234, PSP_CTRL_DOWN,     nullptr},         /* Exit game */
    {1243, PSP_CTRL_CROSS,    nullptr},
    {1290, 0,                 "returned_home"},
    {1298, PSP_CTRL_CIRCLE,   nullptr},         /* browser → console home */
    {1320, 0,                 "home_recent"},
    {1324, PSP_CTRL_DOWN,     nullptr},         /* focus recent game */
    {1344, 0,                 "recent_focus"},
    {1348, PSP_CTRL_UP,       nullptr},         /* return to console row */
    {1396, 0,                 "favorites_home"},
    {1400, PSP_CTRL_CROSS,    nullptr},
    {1420, 0,                 "favorites_list"},
    {1424, PSP_CTRL_CIRCLE,   nullptr},
    {1432, PSP_CTRL_TRIANGLE, nullptr},         /* open Settings */
    {1452, 0,                 "settings"},
    {1456, PSP_CTRL_DOWN,     nullptr},         /* Accent color */
    {1472, 0,                 "accent_picker"},
    {1480, PSP_CTRL_CIRCLE,   nullptr},         /* return to Favorites */
    {1500, PSP_CTRL_CROSS,    nullptr},         /* restore browser focus */
    {1516, PSP_CTRL_CROSS,    nullptr},         /* relaunch selected game */
    {1718, 0,                 "game_relaunch"},
};
constexpr int STEPS = int(sizeof(SCRIPT) / sizeof(SCRIPT[0]));

int  s_frame = 0;
bool s_dirReady = false;
bool s_done = false;
constexpr int TARGET_SYSTEM = RS_AUTOPILOT_SYSTEM_INDEX;
constexpr int targetCategoryForSystem(int system) {
    /* Home keeps PC Engine in the first visible eight cards. */
    return system == 8 ? 7 : (system == 7 ? 8 : system);
}
constexpr int TARGET_CATEGORY = targetCategoryForSystem(TARGET_SYSTEM);
constexpr int FAVORITES_CATEGORY = 9;
constexpr int FAVORITES_NAV_FIRST_FRAME = 1352;
constexpr int FAVORITES_NAV_FRAME_GAP = 4;
constexpr int NAV_FIRST_FRAME = 165;
constexpr int NAV_FRAME_GAP = 8;

}  // namespace

void tick(App& app) {
    if (s_done) {
        s_frame++;
        if (s_frame > SCRIPT[STEPS - 1].frame + 90)
            g_exitRequested = true;
        return;
    }
    if (!s_dirReady) {
        sceIoMkdir(fs::ROOT, 0777);
        sceIoMkdir("ms0:/RETROSHELL/shots", 0777);
        s_dirReady = true;
        RS_LOGI("autopilot: engaged");
    }

    for (int i = 0; i < STEPS; i++) {
        if (SCRIPT[i].frame != s_frame) continue;
        if (SCRIPT[i].press) app.padMutable().simulate(SCRIPT[i].press);
        if (SCRIPT[i].capture) {
            char path[128];
            std::snprintf(path, sizeof path, "ms0:/RETROSHELL/shots/%s.png",
                          SCRIPT[i].capture);
            app.renderer().requestCapture(path);
            RS_LOGI("autopilot: capture %s (app time %d ms)",
                    SCRIPT[i].capture, int(app.time() * 1000.f));
        }
    }

    /* Home starts on the first system (GB, console 0). Move right once per
     * system index. Keeping releases between presses makes this deterministic
     * on both hardware and PPSSPP. */
    for (int i = 0; i < TARGET_CATEGORY; i++) {
        if (s_frame == NAV_FIRST_FRAME + i * NAV_FRAME_GAP)
            app.padMutable().simulate(PSP_CTRL_RIGHT);
    }

    /* After returning from Recent, move from the configured system directly
     * to Favorites. The old script always pressed Right nine times, which
     * only worked for Game Boy and landed later-system runs on Settings. */
    for (int i = TARGET_CATEGORY; i < FAVORITES_CATEGORY; i++) {
        if (s_frame == FAVORITES_NAV_FIRST_FRAME +
                           (i - TARGET_CATEGORY) * FAVORITES_NAV_FRAME_GAP)
            app.padMutable().simulate(PSP_CTRL_RIGHT);
    }

    s_frame++;
    if (s_frame > SCRIPT[STEPS - 1].frame + 30) {
        s_done = true;
        RS_LOGI("autopilot: complete (system index %d)", TARGET_SYSTEM);
    }
}

}  // namespace rs::autopilot

#endif  /* RS_AUTOPILOT */
