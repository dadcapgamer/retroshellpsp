/** RetroShell PSP — entry point.
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
#include <psppower.h>

PSP_MODULE_INFO("RetroShell", 0, 0, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
/* Fixed newlib heap: everything large goes through the arena so the memory
 * map stays deterministic on the 32MB PSP-1000.
 *
 * Measured on real PSP-1000 hardware (beta.15, 15-game library): the heap
 * peaked at 326 KB across boot, browsing and six core launches, against the
 * 4096 KB previously reserved. Every KB reserved here is a KB the arena — and
 * therefore the running emulator core — never sees, so ~3.7 MB was sitting
 * idle for the life of the process.
 *
 * 2048 KB keeps roughly a 6x margin over that measurement rather than trimming
 * to the observed peak: the library index is the one part that scales with the
 * user's collection, and an exhausted newlib heap is a hard failure while a
 * slightly smaller ROM cache is only slower. Re-check the "heap: ... peak"
 * log line against a large library before reducing this further. */
PSP_HEAP_SIZE_KB(2048);

volatile bool g_exitRequested = false;

namespace {

int exitCallback(int, int, void*) {
    g_exitRequested = true;
    return 0;
}

int powerCallback(int, int flags, void*) {
    rs::power::notifyCallback(flags);
    return 0;
}

int callbackThread(SceSize, void*) {
    const int cb = sceKernelCreateCallback("rs_exit_cb", exitCallback, nullptr);
    sceKernelRegisterExitCallback(cb);
    const int powerCb = sceKernelCreateCallback("rs_power_cb", powerCallback,
                                                nullptr);
    if (powerCb >= 0) scePowerRegisterCallback(-1, powerCb);
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
    pspDebugScreenPrintf("RetroShell failed to start:\n  %s\n\n"
                         "Press HOME to quit.", msg);
    while (!g_exitRequested) sceDisplayWaitVblankStart();
    sceKernelExitGame();
    __builtin_unreachable();
}

}  // namespace

int main() {
    setupCallbacks();
    rs::power::setCpuMhz(222);  /* menus don't need 333 */

    if (!rs::mem::init()) fatal("memory arena reservation failed");

    rs::App app;
    if (!app.init()) fatal("renderer / asset init failed");

    app.run();

    app.shutdown();
    RS_LOGI("clean exit (arena high water: %u KB)",
            unsigned(rs::mem::highWater() / 1024));
    rs::mem::shutdown();
    rs::log::shutdown();
    sceKernelExitGame();
    return 0;
}
