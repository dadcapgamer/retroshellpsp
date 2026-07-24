/** Optional per-game metadata and box art.
 *
 * Both are keyed by the ROM's base file name (what users see), so a
 * community metadata pack is just files dropped into:
 *   ms0:/RETROSUITE/metadata/<SystemDir>/<rom name>.json
 *   ms0:/RETROSUITE/boxart/<SystemDir>/<rom name>.png
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

    /* Drop every cached texture (theme switch / core launch eviction). */
    void clear();

private:
    struct Slot {
        u32 hash = 0;
        bool missing = false;   /* negative cache: no file on disk */
        gfx::Texture tex;
    };
    static constexpr int SLOTS = 12;
    Slot m_slots[SLOTS];
    int m_clock = 0;
};

}  // namespace rs::db
