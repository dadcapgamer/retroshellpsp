/** Background ROM scanner.
 *
 * Recursively walks one ms0:/ROMS/ tree on a low-priority worker thread.
 * Console folders are optional: files are assigned to systems by extension.
 * ZIP archives are identified from their first supported ROM entry (central
 * directory only — no decompression), whose CRC32 comes for free.
 * Results are handed to the main thread which swaps them into the GameIndex
 * and refreshes the cache.
 */
#pragma once

#include "frontend/database/game_index.h"

#include <atomic>

namespace rs::db {

class RomScanner {
public:
    /* Kicks off the worker; no-op if already running. */
    void start();
    /* Requests cancellation and joins the worker. Safe if already stopped. */
    void stop();

    bool running() const   { return m_running.load(); }
    /* Files inspected so far (progress feedback). */
    int  progress() const  { return m_progress.load(); }

    /* Main-thread poll: returns true once per finished scan and moves the
     * results out. */
    bool takeResults(std::vector<GameEntry>& out);

private:
    static int threadMain(void* self);
    void scan();
    void scanDirectory(const std::string& directory, int depth);

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_done{false};
    std::atomic<bool> m_stopRequested{false};
    std::atomic<int>  m_progress{0};
    int m_threadId = -1;
    std::vector<GameEntry> m_results;
};

}  // namespace rs::db
