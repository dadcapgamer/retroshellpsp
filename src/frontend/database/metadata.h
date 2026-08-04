/** Optional per-game metadata and box art.
 *
 * Box art first follows the ROM itself:
 *   ms0:/ROMS/.../<rom name>.(png|jpg|jpeg)
 * with the same base name as the ROM. Metadata can remain centralized;
 * artwork is indexed beside ROMs so coverless entries require no failed
 * filesystem probes during browsing.
 *
 * Box art decoding happens synchronously but is throttled to one image per
 * frame and cached; textures are dropped wholesale when the launch protocol
 * evicts frontend assets.
 */
#pragma once

#include "frontend/database/game_index.h"
#include "platform/psp/gu_renderer.h"

#include <string>

namespace rs::db {

struct GameMeta {
    std::string description, developer, publisher, genre;
    int year = 0;
    bool loaded = false;   /* file existed and parsed */
};

GameMeta loadMeta(const GameEntry& g);

class BoxartCache {
public:
    /* Returns the texture for this game, kicking off at most one decode per
     * call. Never blocks scrolling: returns nullptr until ready. */
    const gfx::Texture* get(const GameEntry& g);
    /* Cache-only lookup used by multi-thumbnail shelves. It never touches
     * the Memory Stick or decodes an image. */
    const gfx::Texture* peek(const GameEntry& g) const;

    /* Drop every cached texture (theme switch / core launch eviction). */
    void clear();

private:
    struct Slot {
        u32 hash = 0;
        bool missing = false;   /* negative cache: no file on disk */
        gfx::Texture tex;
    };
    static constexpr int SLOTS = 12;
    static constexpr int MISSING_SLOTS = 192;
    Slot m_slots[SLOTS];
    u32 m_missing[MISSING_SLOTS] = {};
    int m_clock = 0;
    int m_missingClock = 0;
};

}  // namespace rs::db
