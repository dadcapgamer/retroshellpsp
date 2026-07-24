/** RetroSuite PSP — entry point.
 *
 * Owns the PSP module boilerplate and HOME-menu exit callback; everything
 * else lives in App (src/frontend/app.*).
 */
#include "frontend/app.h"
#include "platform/psp/power.h"
#include "runtime/arena.h"
#include "runtime/log.h"

#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspkernel.h>

PSP_MODULE_INFO("RetroSuite", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
/* Fixed 4MB newlib heap: everything large goes through the arena so the
 * memory map stays deterministic on the 32MB PSP-1000. */
PSP_HEAP_SIZE_KB(4096);

volatile bool g_exitRequested = false;

namespace {

int exitCallback(int, int, void*) {
    g_exitRequested = true;
    return 0;
}

int callbackThread(SceSize, void*) {
    const int cb = sceKernelCreateCallback("rs_exit_cb", exitCallback, nullptr);
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

void setupCallbacks() {
    const int thid = sceKernelCreateThread("rs_callbacks", callbackThread,
                                           0x11, 0x1000, 0, nullptr);
    if (thid >= 0) sceKernelStartThread(thid, 0, nullptr);
}

[[noreturn]] void fatal(const char* msg) {
    pspDebugScreenInit();
    pspDebugScreenPrintf("RetroSuite failed to start:\n  %s\n\n"
                         "Press HOME to quit.", msg);
    while (!g_exitRequested) sceDisplayWaitVblankStart();
    sceKernelExitGame();
    __builtin_unreachable();
}

}  // namespace

int main() {
    setupCallbacks();
    rs::log::init(/*toFile=*/true);
    RS_LOGI("RetroSuite starting");

    rs::power::setCpuMhz(222);  /* menus don't need 333 */

    if (!rs::mem::init()) fatal("memory arena reservation failed");

    rs::App app;
    if (!app.init()) fatal("renderer / asset init failed");

    app.run();

    RS_LOGI("clean exit (arena high water: %u KB)",
            unsigned(rs::mem::highWater() / 1024));
    app.shutdown();
    rs::mem::shutdown();
    rs::log::shutdown();
    sceKernelExitGame();
    return 0;
}
