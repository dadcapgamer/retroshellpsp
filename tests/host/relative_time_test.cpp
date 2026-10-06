#include "src/frontend/ui/relative_time.h"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
void expect(u64 stamp, u64 now, const char* want) {
    char out[32];
    rs::ui::formatRelative(stamp, now, out, sizeof out);
    if (std::strcmp(out, want) != 0) {
        std::fprintf(stderr, "stamp %llu now %llu: got '%s', want '%s'\n",
                     (unsigned long long)stamp, (unsigned long long)now, out,
                     want);
        assert(false);
    }
}
}  // namespace

int main() {
    const u64 now = 202610021446ull;   /* 2026-10-02 14:46 */

    expect(0, now, "Never");
    expect(202610021446ull, now, "Just now");
    expect(202610021445ull, now, "1m ago");
    expect(202610021416ull, now, "30m ago");
    expect(202610021246ull, now, "2h ago");          /* the mockup's "2h ago" */
    expect(202610011446ull, now, "1d ago");          /* exactly a day */
    expect(202609291446ull, now, "3d ago");
    expect(202609271446ull, now, "5d ago");

    /* Month and year boundaries use real calendar distance. */
    expect(202610010000ull, 202610020000ull, "1d ago");
    expect(202509301446ull, now, "1y ago");
    expect(202605021446ull, now, "5mo ago");
    expect(202512312345ull, 202601010005ull, "20m ago");   /* across new year */
    expect(202402281200ull, 202403011200ull, "2d ago");     /* leap day */

    /* Malformed stamps and a clock that moved backwards stay safe. */
    expect(202613011200ull, now, "Unknown");
    expect(202610321200ull, now, "Unknown");
    expect(202610022560ull, now, "Unknown");
    expect(12345ull, now, "Unknown");
    expect(202610021500ull, now, "Just now");        /* played "in the future" */
    expect(202610021446ull, 0, "Unknown");

    char tiny[4];
    rs::ui::formatRelative(202610021246ull, now, tiny, sizeof tiny);
    assert(std::strlen(tiny) == 3);                    /* truncated, terminated */
    rs::ui::formatRelative(202610021246ull, now, tiny, 0);   /* no-op, no crash */

    auto playtime = [](u32 seconds, const char* want) {
        char out[32];
        rs::ui::formatPlaytime(seconds, out, sizeof out);
        if (std::strcmp(out, want) != 0) {
            std::fprintf(stderr, "playtime %u: got '%s', want '%s'\n", seconds,
                         out, want);
            assert(false);
        }
    };
    playtime(0, "Under 1 min");
    playtime(59, "Under 1 min");
    playtime(60, "1m");
    playtime(2520, "42m");
    playtime(3599, "59m");
    playtime(3600, "1h 00m");
    playtime(11100, "3h 05m");
    playtime(4000000000u, "1111111h 06m");
    return 0;
}
