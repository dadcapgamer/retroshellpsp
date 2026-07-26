#include "platform/psp/threading.h"

#include <pspkernel.h>
#include <pspthreadman.h>

namespace rs::thread {

namespace {
struct Trampoline {
    Fn fn;
    void* arg;
};

int threadEntry(SceSize, void* argp) {
    /* argp points at a Trampoline copied by value into kernel space. */
    Trampoline t = **static_cast<Trampoline**>(argp);
    delete *static_cast<Trampoline**>(argp);
    return t.fn(t.arg);
}
}  // namespace

int spawn(const char* name, Fn fn, void* arg, int stackKb) {
    /* Priority 0x20 < main thread's 0x11-ish urgency — scans must never
     * steal frame time. */
    const SceUID tid = sceKernelCreateThread(name, threadEntry, 0x20,
                                             stackKb * 1024,
                                             PSP_THREAD_ATTR_USER, nullptr);
    if (tid < 0) return int(tid);
    auto* t = new Trampoline{fn, arg};
    if (!t) {
        sceKernelDeleteThread(tid);
        return -1;
    }
    if (sceKernelStartThread(tid, sizeof(void*), &t) < 0) {
        delete t;
        sceKernelDeleteThread(tid);
        return -1;
    }
    return int(tid);
}

void join(int threadId) {
    if (threadId < 0) return;
    sceKernelWaitThreadEnd(SceUID(threadId), nullptr);
    sceKernelDeleteThread(SceUID(threadId));
}

Mutex::Mutex() { m_sema = sceKernelCreateSema("rs_mutex", 0, 1, 1, nullptr); }
Mutex::~Mutex() {
    if (m_sema >= 0) sceKernelDeleteSema(m_sema);
}
void Mutex::lock() { sceKernelWaitSema(m_sema, 1, nullptr); }
void Mutex::unlock() { sceKernelSignalSema(m_sema, 1); }

void sleepMs(int ms) { sceKernelDelayThread(ms * 1000); }

}  // namespace rs::thread
