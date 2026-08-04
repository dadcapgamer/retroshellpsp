#pragma once

#include <cctype>
#include <string>

namespace rs::db {

/* Human-friendly, ASCII case-insensitive ordering for ROM names. Numeric
 * runs compare by value, so "Game 9" appears before "Game 10". */
inline bool naturalNameLess(const std::string& a, const std::string& b) {
    size_t ai = 0, bi = 0;
    while (ai < a.size() && bi < b.size()) {
        const unsigned char ac = static_cast<unsigned char>(a[ai]);
        const unsigned char bc = static_cast<unsigned char>(b[bi]);
        if (std::isdigit(ac) && std::isdigit(bc)) {
            size_t az = ai, bz = bi;
            while (az < a.size() && a[az] == '0') ++az;
            while (bz < b.size() && b[bz] == '0') ++bz;
            size_t ae = az, be = bz;
            while (ae < a.size() && std::isdigit(
                       static_cast<unsigned char>(a[ae]))) ++ae;
            while (be < b.size() && std::isdigit(
                       static_cast<unsigned char>(b[be]))) ++be;
            const size_t alen = ae - az, blen = be - bz;
            if (alen != blen) return alen < blen;
            const int numeric = a.compare(az, alen, b, bz, blen);
            if (numeric != 0) return numeric < 0;
            size_t arun = ai, brun = bi;
            while (arun < a.size() && std::isdigit(
                       static_cast<unsigned char>(a[arun]))) ++arun;
            while (brun < b.size() && std::isdigit(
                       static_cast<unsigned char>(b[brun]))) ++brun;
            ai = arun;
            bi = brun;
            continue;
        }
        const unsigned char af = static_cast<unsigned char>(std::tolower(ac));
        const unsigned char bf = static_cast<unsigned char>(std::tolower(bc));
        if (af != bf) return af < bf;
        ++ai;
        ++bi;
    }
    if (ai != a.size() || bi != b.size()) return ai == a.size();
    return a < b;  /* deterministic tie-break for capitalization/zero padding */
}

}  // namespace rs::db
