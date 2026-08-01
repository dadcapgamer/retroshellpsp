/** User library state: favorites, recently played, play counts/timestamps.
 * Persisted as small JSON (ms0:/RETROSHELL/library.json), keyed by the
 * game's stable pathHash.
 */
#pragma once

#include "rs_common.h"

#include <vector>

namespace rs::db {

class Library {
public:
    void load();
    void save() const;

    bool isFavorite(u32 hash) const;
    void toggleFavorite(u32 hash);

    /* Most recent first, capped. */
    const std::vector<u32>& recents() const { return m_recents; }
    const std::vector<u32>& favorites() const { return m_favorites; }
    void notePlayed(u32 hash, u64 localTimestamp);

    int playCount(u32 hash) const;
    /* YYYYMMDDHHMM, or 0 for libraries created before timestamp tracking. */
    u64 lastPlayed(u32 hash) const;

private:
    static constexpr int MAX_RECENTS = 20;

    std::vector<u32> m_favorites;
    std::vector<u32> m_recents;
    std::vector<std::pair<u32, int>> m_playCounts;
    std::vector<std::pair<u32, u64>> m_lastPlayed;
    bool m_dirty = false;
};

}  // namespace rs::db
