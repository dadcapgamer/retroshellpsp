#include "src/frontend/database/library_view.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>

using namespace rs::db;

namespace {
int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
    failures++; } } while (0)

struct FakeLib {
    std::set<u32> favorites;
    std::map<u32, u64> played;
    std::map<u32, int> counts;
    bool isFavorite(u32 h) const { return favorites.count(h) != 0; }
    u64 lastPlayed(u32 h) const { auto i = played.find(h); return i == played.end() ? 0 : i->second; }
    int playCount(u32 h) const { auto i = counts.find(h); return i == counts.end() ? 0 : i->second; }
};

GameEntry game(const char* title, u32 hash, u32 mtime) {
    GameEntry g;
    g.name = title;
    g.title = title;
    g.label = title;
    g.pathHash = hash;
    g.mtime = mtime;
    return g;
}

std::vector<std::string> names(const std::vector<const GameEntry*>& v) {
    std::vector<std::string> out;
    for (auto* g : v) out.push_back(g->title);
    return out;
}
}  // namespace

int main() {
    /* A-Z input, as GameIndex produces. */
    const std::vector<GameEntry> games = {
        game("Alpha", 1, 10), game("Bravo", 2, 50), game("Charlie", 3, 30),
        game("Delta", 4, 40), game("Pok\xC3\xA9mon Crystal", 5, 20),
    };
    FakeLib lib;
    lib.favorites = {2, 4};
    lib.played = {{3, 202610010900ull}, {1, 202610021200ull}};
    lib.counts = {{3, 9}, {1, 2}, {4, 5}};

    /* Default view is exactly the input order. */
    auto all = buildView(games, {}, "", lib);
    CHECK(all.size() == 5 && all[0]->pathHash == 1 && all[4]->pathHash == 5);

    /* Filters. */
    ViewState fav{ViewFilter::Favorites, ViewSort::NameAZ};
    CHECK((names(buildView(games, fav, "", lib)) ==
           std::vector<std::string>{"Bravo", "Delta"}));

    ViewState added{ViewFilter::RecentlyAdded, ViewSort::NameAZ};
    auto recent = buildView(games, added, "", lib);
    CHECK(recent[0]->pathHash == 2 && recent[1]->pathHash == 4 &&
          recent[4]->pathHash == 1);

    /* Sorts. */
    ViewState za{ViewFilter::All, ViewSort::NameZA};
    CHECK(buildView(games, za, "", lib)[0]->pathHash == 5);

    ViewState byPlayed{ViewFilter::All, ViewSort::RecentlyPlayed};
    auto played = buildView(games, byPlayed, "", lib);
    CHECK(played[0]->pathHash == 1 && played[1]->pathHash == 3);
    CHECK(played[2]->pathHash == 2);           /* unplayed keep A-Z order */

    ViewState most{ViewFilter::All, ViewSort::MostPlayed};
    auto top = buildView(games, most, "", lib);
    CHECK(top[0]->pathHash == 3 && top[1]->pathHash == 4 && top[2]->pathHash == 1);

    /* Filter and sort compose. */
    ViewState favMost{ViewFilter::Favorites, ViewSort::MostPlayed};
    CHECK(buildView(games, favMost, "", lib)[0]->pathHash == 4);

    /* Search: case-insensitive substring, accent-blind both ways. */
    CHECK(buildView(games, {}, "CHAR", lib).size() == 1);
    CHECK(buildView(games, {}, "pokemon", lib).size() == 1);
    CHECK(buildView(games, {}, "pok\xC3\xA9mon", lib).size() == 1);
    CHECK(buildView(games, {}, "zzz", lib).empty());
    CHECK(buildView(games, fav, "a", lib).size() == 2);   /* Bravo, Delta */

    /* Recently Added is capped. */
    std::vector<GameEntry> many;
    for (u32 i = 0; i < 100; i++) many.push_back(game("G", i + 1, i));
    CHECK(int(buildView(many, added, "", lib).size()) == RECENTLY_ADDED_MAX);

    /* Persisted encoding round-trips and rejects garbage. */
    for (int f = 0; f < int(ViewFilter::Count); f++)
        for (int s = 0; s < int(ViewSort::Count); s++) {
            ViewState v{ViewFilter(f), ViewSort(s)};
            CHECK(ViewState::unpack(v.pack()) == v);
        }
    CHECK(ViewState::unpack(-1).isDefault());
    CHECK(ViewState::unpack(9999).isDefault());
    CHECK(ViewState::unpack(0x0F).isDefault());     /* sort out of range */

    /* Selection survives a view change by hash. */
    CHECK(indexOfHash(buildView(games, fav, "", lib), 4) == 1);
    CHECK(indexOfHash(buildView(games, fav, "", lib), 1) == -1);
    CHECK(indexOfHash(all, 0) == -1);

    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
