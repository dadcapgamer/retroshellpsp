/** Relative "last played" strings ("2h ago") from YYYYMMDDHHMM stamps.
 *
 * Pure and header-only so the host test suite can exercise it without the
 * PSP SDK. Stamps are the library's local-time format; no timezone is
 * involved because both operands come from the same RTC.
 */
#pragma once

#include "rs_common.h"

#include <cstdio>

namespace rs::ui {

namespace detail {

/* Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant). */
inline s64 daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const s64 era = (y >= 0 ? y : y - 399) / 400;
    const s64 yoe = y - era * 400;
    const s64 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const s64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* Minutes since the epoch, or false for a malformed stamp. */
inline bool stampMinutes(u64 stamp, s64& out) {
    const int minute = int(stamp % 100u);
    const int hour   = int((stamp / 100u) % 100u);
    const int day    = int((stamp / 10000u) % 100u);
    const int month  = int((stamp / 1000000u) % 100u);
    const int year   = int(stamp / 100000000u);
    if (year < 2000 || year > 2199 || month < 1 || month > 12 || day < 1 ||
        day > 31 || hour > 23 || minute > 59)
        return false;
    out = (daysFromCivil(year, month, day) * 24 + hour) * 60 + minute;
    return true;
}

}  // namespace detail

/* "Never", "Unknown", "Just now", "12m ago", "2h ago", "3d ago", "5mo ago",
 * "2y ago". A stamp in the future (RTC changed) reads "Just now". */
inline void formatRelative(u64 stamp, u64 now, char* out, size_t n) {
    if (n == 0) return;
    if (stamp == 0) {
        std::snprintf(out, n, "Never");
        return;
    }
    s64 then = 0, current = 0;
    if (!detail::stampMinutes(stamp, then) ||
        !detail::stampMinutes(now, current)) {
        std::snprintf(out, n, "Unknown");
        return;
    }
    const s64 minutes = current - then;
    if (minutes < 1) std::snprintf(out, n, "Just now");
    else if (minutes < 60) std::snprintf(out, n, "%dm ago", int(minutes));
    else if (minutes < 60 * 24)
        std::snprintf(out, n, "%dh ago", int(minutes / 60));
    else if (minutes < 60 * 24 * 30)
        std::snprintf(out, n, "%dd ago", int(minutes / (60 * 24)));
    else if (minutes < 60 * 24 * 365)
        std::snprintf(out, n, "%dmo ago", int(minutes / (60 * 24 * 30)));
    else
        std::snprintf(out, n, "%dy ago", int(minutes / (60 * 24 * 365)));
}

/* Time spent in a game: "Under 1 min", "42m", "3h 05m". Seconds are
 * rounded down; a game that has been run but barely played still reads
 * honestly rather than "0m". */
inline void formatPlaytime(u32 seconds, char* out, size_t n) {
    if (n == 0) return;
    if (seconds < 60) std::snprintf(out, n, "Under 1 min");
    else if (seconds < 3600) std::snprintf(out, n, "%um", unsigned(seconds / 60u));
    else
        std::snprintf(out, n, "%uh %02um", unsigned(seconds / 3600u),
                      unsigned((seconds % 3600u) / 60u));
}

}  // namespace rs::ui
