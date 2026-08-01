/** Thin wrappers over PSP kernel threads and semaphores.
 *
 * RetroShell uses exactly one background worker (the library scanner /
 * asset loader); emulation itself always runs on the main thread.
 */
#pragma once

#include "rs_common.h"

namespace rs::thread {

/* Runs fn(arg) once on a low-priority kernel thread. Returns a PSP thread
 * id, or a negative error. The owner must join every successful spawn. */
using Fn = int (*)(void* arg);
int spawn(const char* name, Fn fn, void* arg, int stackKb = 64);
void join(int threadId);

class Mutex {
public:
    Mutex();
    ~Mutex();
    void lock();
    void unlock();

private:
    int m_sema = -1;
};

class ScopedLock {
public:
    explicit ScopedLock(Mutex& m) : m_m(m) { m_m.lock(); }
    ~ScopedLock() { m_m.unlock(); }

private:
    Mutex& m_m;
};

void sleepMs(int ms);

}  // namespace rs::thread
