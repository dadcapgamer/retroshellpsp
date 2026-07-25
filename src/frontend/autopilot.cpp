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
/* Clean render test: launch the SNES game via snes9x2005 (the only SNES
 * core once dummy is removed → direct launch, no picker) and capture raw
 * gameplay frames with NO input, so nothing opens the in-game menu. */
constexpr Step SCRIPT[] = {
    {30,   0,                 "boot"},
    {150,  0,                 "home"},
    {165,  PSP_CTRL_RIGHT,    nullptr},   /* GB → GBC */
    {173,  PSP_CTRL_RIGHT,    nullptr},   /* → GBA */
    {181,  PSP_CTRL_RIGHT,    nullptr},   /* → NES */
    {189,  PSP_CTRL_RIGHT,    nullptr},   /* → SNES */
    {205,  PSP_CTRL_DOWN,     nullptr},   /* into the SNES list */
    {240,  0,                 "list_snes"},
    {260,  PSP_CTRL_CROSS,    nullptr},   /* single SNES core → direct launch */
    {560,  0,                 "game_a"},
    {820,  0,                 "game_b"},
    {1080, 0,                 "game_c"},
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
