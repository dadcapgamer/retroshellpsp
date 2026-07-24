/** User library state: favorites, recently played, play counts.
 * Persisted as small JSON (ms0:/RETROSUITE/library.json), keyed by the
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
    void notePlayed(u32 hash);

    int playCount(u32 hash) const;

private:
    static constexpr int MAX_RECENTS = 20;

    std::vector<u32> m_favorites;
    std::vector<u32> m_recents;
    std::vector<std::pair<u32, int>> m_playCounts;
    bool m_dirty = false;
};

}  // namespace rs::db
