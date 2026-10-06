/** Library row thumbnails: 24x24 RGB565, one cache file per system
 * (ms0:/RETROSHELL/cache/thumbs/<System>.bin).
 *
 * All Memory Stick work happens on a background worker, never on a frame:
 * the worker builds missing thumbnails from each game's cover (or the
 * player's newest screenshot) and loads the active system's file when the
 * Library switches to it. The main thread only binary-searches the loaded
 * set and uploads at most a handful of 1 KB textures for the visible rows,
 * so scrolling never touches the stick and a system with no thumbnails yet
 * simply shows its console glyph until they arrive.
 */
#pragma once

#include "frontend/database/game_index.h"
#include "platform/psp/gu_renderer.h"
#include "rs_common.h"

#include <atomic>
#include <string>
#include <vector>

namespace rs::db {

class ThumbCache {
public:
    static constexpr int SIZE = 24;
    static constexpr int PIXELS = SIZE * SIZE;
    /* Per system, in RAM at once: ~1.2 KB each, so about 520 KB at most. */
    static constexpr int MAX_PER_SYSTEM = 448;

    /* Queue a (re)build of every system's cache file from `index`. With
     * `force`, existing thumbnails are redone (an explicit rescan, where
     * covers may have been replaced). */
    void rebuild(const GameIndex& index, bool force);
    /* The Library is showing `system`: make its thumbnails resident. */
    void requestSystem(int system);
    /* Main-thread pump: adopts finished work and starts the next job. */
    void update();
    /* Stops the worker and frees everything (core launch, shutdown). */
    void suspend();

    /* Texture for a game in the resident system, or nullptr. */
    const gfx::Texture* get(u32 pathHash);

    bool busy() const { return m_threadId >= 0; }

private:
    struct Source {
        u32 hash = 0;
        std::string path;        /* cover or screenshot */
    };
    struct Job {
        enum Kind : u8 { Build, Load } kind = Load;
        int system = 0;
        bool force = false;
        std::vector<Source> sources;            /* Build only */
    };
    struct Entry {
        u32 hash;
        u16 px[PIXELS];
    };

    static int threadMain(void* self);
    void runJob();
    void startNext();
    void dropResident();

    /* Worker side. */
    Job m_job;
    std::vector<Entry> m_loaded;                /* Load result */
    std::atomic<bool> m_done{false};
    std::atomic<bool> m_stop{false};
    int m_threadId = -1;

    /* Main thread. */
    std::vector<Job> m_queue;
    std::vector<Entry> m_resident;
    int m_residentSystem = -1;
    int m_wantSystem = -1;
    u32 m_generation = 0;                       /* bumps when a build lands */
    u32 m_residentGeneration = 0;

    static constexpr int RING = 16;
    struct Slot {
        u32 hash = 0;
        u32 lastUse = 0;
        gfx::Texture tex;
    };
    Slot m_ring[RING];
    u32 m_clock = 0;
};

}  // namespace rs::db
