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
 * Do not trim this without measuring a *screenshot*. A capture is the
 * application's largest heap burst by a wide margin — a 255 KB framebuffer
 * snapshot, a 383 KB RGB image, and the PNG encoder's zlib buffers on top —
 * and it is transient, so it never appears in a sample taken at core launch.
 *
 * Measured on the PPSSPP PSP-1000 model: 511 KB in use at core launch but
 * 1277 KB in use and 1536 KB claimed during a capture. A previous reduction
 * to 2048 KB was made from the core-launch figure alone and left too little
 * room for the capture's 383 KB contiguous request once the heap had
 * fragmented; QuickNES then froze mid-game on the first in-game screenshot,
 * silently, because the PSP has no exception handler to report it.
 *
 * 4096 KB is roughly 2.7x the measured peak claim. The cost is arena space —
 * every KB here is a KB the running core never sees — so the correct way to
 * reclaim it is to stop the capture path allocating from this heap, not to
 * lower the number again. */
PSP_HEAP_SIZE_KB(4096);

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
