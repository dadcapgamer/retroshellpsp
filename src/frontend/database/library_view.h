/** Library view: which games a system's list shows, and in what order.
 *
 * Filter, sort and search are deliberately secondary controls — the primary
 * flow stays Continue / Systems / Library / Play — so the defaults (All Games,
 * A-Z, no query) reproduce the plain alphabetical list exactly.
 *
 * Pure and header-only (no PSP dependencies) so the host tests pin the rules.
 */
#pragma once

#include "frontend/database/game_index.h"
#include "rs_common.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace rs::db {

enum class ViewFilter : u8 { All, Favorites, RecentlyAdded, Count };
enum class ViewSort : u8 { NameAZ, NameZA, RecentlyPlayed, MostPlayed, Count };

struct ViewState {
    ViewFilter filter = ViewFilter::All;
    ViewSort   sort   = ViewSort::NameAZ;

    bool isDefault() const {
        return filter == ViewFilter::All && sort == ViewSort::NameAZ;
    }
    bool operator==(const ViewState& o) const {
        return filter == o.filter && sort == o.sort;
    }
    /* One small integer for library.json. */
    int  pack() const { return int(filter) * 16 + int(sort); }
    static ViewState unpack(int packed) {
        ViewState v;
        const int f = packed / 16, s = packed % 16;
        if (packed >= 0 && f < int(ViewFilter::Count) &&
            s < int(ViewSort::Count)) {
            v.filter = ViewFilter(f);
            v.sort = ViewSort(s);
        }
        return v;
    }
};

constexpr int RECENTLY_ADDED_MAX = 30;

inline const char* filterName(ViewFilter f) {
    switch (f) {
        case ViewFilter::Favorites:     return "Favorites";
        case ViewFilter::RecentlyAdded: return "Recently Added";
        default:                        return "All Games";
    }
}

inline const char* sortName(ViewSort s) {
    switch (s) {
        case ViewSort::NameZA:         return "Z-A";
        case ViewSort::RecentlyPlayed: return "Recently Played";
        case ViewSort::MostPlayed:     return "Most Played";
        default:                       return "A-Z";
    }
}

/* ASCII-lowercases and strips Latin-1 accents (UTF-8 C3 xx) so that typing
 * "pokemon" finds "Pokémon". Anything else passes through unchanged. */
inline std::string searchFold(const std::string& s) {
    static const char* LATIN1 =   /* U+00C0..U+00FF -> base letter, 0 = keep */
        "AAAAAAACEEEEIIII" "DNOOOOO\0OUUUUY\0\0"
        "aaaaaaaceeeeiiii" "dnooooo\0ouuuuy\0y";
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0xC3 && i + 1 < s.size()) {
            const unsigned char d = static_cast<unsigned char>(s[i + 1]);
            if (d >= 0x80 && d <= 0xBF && LATIN1[d - 0x80]) {
                out.push_back(char(std::tolower(
                    static_cast<unsigned char>(LATIN1[d - 0x80]))));
                i++;
                continue;
            }
        }
        out.push_back(char(std::tolower(c)));
    }
    return out;
}

inline bool matchesQuery(const std::string& label, const std::string& query) {
    if (query.empty()) return true;
    return searchFold(label).find(searchFold(query)) != std::string::npos;
}

/* `lib` supplies isFavorite(hash), lastPlayed(hash) and playCount(hash).
 * `games` must already be in A-Z order (GameIndex guarantees it). */
template <class Lib>
std::vector<const GameEntry*> buildView(const std::vector<GameEntry>& games,
                                        const ViewState& view,
                                        const std::string& query,
                                        const Lib& lib) {
    std::vector<const GameEntry*> out;
    out.reserve(games.size());
    const std::string needle = searchFold(query);
    for (const GameEntry& g : games) {
        if (view.filter == ViewFilter::Favorites &&
            !lib.isFavorite(g.pathHash))
            continue;
        if (!needle.empty() &&
            searchFold(g.shown()).find(needle) == std::string::npos)
            continue;
        out.push_back(&g);
    }

    if (view.filter == ViewFilter::RecentlyAdded) {
        /* Newest file first; a bulk copy stamps everything alike, so fall
         * back to the A-Z order the input already has. */
        std::stable_sort(out.begin(), out.end(),
                         [](const GameEntry* a, const GameEntry* b) {
                             return a->mtime > b->mtime;
                         });
        if (int(out.size()) > RECENTLY_ADDED_MAX)
            out.resize(size_t(RECENTLY_ADDED_MAX));
        return out;
    }

    switch (view.sort) {
        case ViewSort::NameZA:
            std::reverse(out.begin(), out.end());
            break;
        case ViewSort::RecentlyPlayed: {
            std::vector<std::pair<u64, const GameEntry*>> keyed;
            keyed.reserve(out.size());
            for (const GameEntry* g : out)
                keyed.push_back({lib.lastPlayed(g->pathHash), g});
            std::stable_sort(keyed.begin(), keyed.end(),
                             [](const auto& a, const auto& b) {
                                 return a.first > b.first;
                             });
            for (size_t i = 0; i < out.size(); i++) out[i] = keyed[i].second;
            break;
        }
        case ViewSort::MostPlayed: {
            std::vector<std::pair<int, const GameEntry*>> keyed;
            keyed.reserve(out.size());
            for (const GameEntry* g : out)
                keyed.push_back({lib.playCount(g->pathHash), g});
            std::stable_sort(keyed.begin(), keyed.end(),
                             [](const auto& a, const auto& b) {
                                 return a.first > b.first;
                             });
            for (size_t i = 0; i < out.size(); i++) out[i] = keyed[i].second;
            break;
        }
        default:
            break;
    }
    return out;
}

/* Position of `hash` in a built view, or -1. */
inline int indexOfHash(const std::vector<const GameEntry*>& view, u32 hash) {
    if (!hash) return -1;
    for (size_t i = 0; i < view.size(); i++)
        if (view[i]->pathHash == hash) return int(i);
    return -1;
}

}  // namespace rs::db
