/** User library state: favorites, recently played, play counts/timestamps.
 * Persisted as small JSON (ms0:/RETROSHELL/library.json), keyed by the
 * game's stable pathHash.
 */
#pragma once

#include "frontend/database/library_view.h"
#include "rs_common.h"

#include <unordered_map>
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
    /* "Remove from Recent": drops the entry only; play count and timestamp
     * stay so the game's history is not rewritten. */
    bool removeRecent(u32 hash);

    /* Last game highlighted in each system's Library, by db::System id.
     * Updating is memory-only (browsing must not touch the Memory Stick);
     * flush() persists it when the user leaves Home. */
    u32 lastSelected(int systemIdx) const;
    void setLastSelected(int systemIdx, u32 hash);
    void flush();

    /* Bumped whenever the recents list changes, so views can refresh without
     * comparing the whole list every frame. */
    u32 recentsRevision() const { return m_recentsRevision; }

    /* Filter and sort chosen for a system's Library (db::System id). Like
     * lastSelected it is memory-only until flush(). */
    ViewState view(int systemIdx) const;
    void setView(int systemIdx, ViewState v);

    /* Where the user was in the shell — rail system, layer and Continue
     * slot — so a native emulator (which replaces the whole process) or a
     * power cycle still returns them to the same place. Layer is the
     * nav::Layer value; the shell clamps whatever it reads back. */
    struct Location {
        int system = 0;
        int layer = 0;
        int continueIdx = 0;
        bool valid = false;
    };
    const Location& location() const { return m_location; }
    void setLocation(int system, int layer, int continueIdx);

    /* Whole seconds spent running the game (menu time excluded). */
    void addPlaytime(u32 hash, u32 seconds);
    u32  playSeconds(u32 hash) const;

    int playCount(u32 hash) const;
    /* YYYYMMDDHHMM, or 0 for libraries created before timestamp tracking. */
    u64 lastPlayed(u32 hash) const;

private:
    static constexpr int MAX_RECENTS = 20;

    std::vector<u32> m_favorites;
    std::vector<u32> m_recents;
    /* Hashed: sorting a 2,000-game library by play history would otherwise
     * do millions of linear probes on the 222 MHz menu clock. */
    std::unordered_map<u32, int> m_playCounts;
    std::unordered_map<u32, u64> m_lastPlayed;
    std::unordered_map<u32, u32> m_playSeconds;
    std::vector<std::pair<int, u32>> m_lastSelected;
    std::vector<std::pair<int, ViewState>> m_views;
    Location m_location;
    u32  m_recentsRevision = 0;
    bool m_dirty = false;   /* unsaved lastSelected change */
};

}  // namespace rs::db
