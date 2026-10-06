/** The in-memory game library and its binary cache.
 *
 * The cache (ms0:/RETROSHELL/cache/index.bin) makes cold boot instant: the
 * previous library is shown immediately while the scanner revalidates in
 * the background and swaps in changes.
 */
#pragma once

#include "frontend/database/systems.h"
#include "rs_common.h"

#include <string>
#include <vector>

namespace rs::db {

struct GameEntry {
    std::string name;       /* file name sans extension — the key for
                             * metadata/art lookup, never rewritten      */
    std::string title;      /* cleaned display title (see title_clean.h) */
    std::string variant;    /* "USA, Rev 1": the dump tags removed from
                             * the title                                 */
    std::string label;      /* what lists show: the title, with the
                             * variant appended only when two games in the
                             * same system would otherwise look identical */
    std::string path;       /* full ms0:/ path (the .zip when zipped)    */
    std::string zipEntry;   /* rom name inside the zip, or empty         */
    std::string artPath;    /* sibling cover discovered during scan      */
    System system  = System::GameBoy;
    u32 pathHash   = 0;     /* FNV-1a of path — stable library key       */
    u32 crc32      = 0;     /* from zip central dir; 0 = not yet known   */
    u32 size       = 0;     /* uncompressed rom size                     */
    u32 mtime      = 0;

    /* Never blank: the label, then the title, then the raw file name. */
    const std::string& shown() const {
        return !label.empty() ? label : (!title.empty() ? title : name);
    }
};

u32 fnv1a(const char* s);

class GameIndex {
public:
    /* Games for one system, sorted by name. */
    const std::vector<GameEntry>& games(System s) const {
        return m_bySystem[u8(s)];
    }
    int totalCount() const;

    /* Bumped on every replaceAll. Views holding GameEntry* (e.g. the home
     * list) must invalidate on a generation change, NOT on a count change:
     * a rescan can keep the count identical while freeing and rebuilding
     * every entry, which would dangle their pointers. */
    u32 generation() const { return m_generation; }

    const GameEntry* byHash(u32 pathHash) const;

    /* Wholesale replacement from a finished scan (main thread only). */
    void replaceAll(std::vector<GameEntry> all);

    bool loadCache();
    bool saveCache() const;

private:
    std::vector<GameEntry> m_bySystem[SYSTEM_COUNT];
    u32 m_generation = 0;
};

}  // namespace rs::db
