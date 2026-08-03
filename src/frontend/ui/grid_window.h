#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rs::ui {

struct GridWindow {
    int first = 0;
    int pastLast = 0;
};

/* Returns only the items that can intersect the viewport. `scrollRow` may
 * be fractional while the list animates, so ceil() preserves partially
 * visible rows at both edges without walking the full library. */
inline GridWindow visibleGridWindow(int itemCount, int columns,
                                    int visibleRows, float scrollRow) {
    if (itemCount <= 0 || columns <= 0 || visibleRows <= 0)
        return {};
    if (!(scrollRow >= 0.f)) scrollRow = 0.f;  /* negative and NaN */

    const int totalRows = (itemCount + columns - 1) / columns;
    scrollRow = std::min(scrollRow, float(totalRows));
    const int firstRow =
        std::max(0, int(std::ceil(scrollRow - 1.f)));
    const int pastLastRow =
        std::min(totalRows, int(std::ceil(scrollRow + visibleRows)));

    const std::int64_t first = std::int64_t(firstRow) * columns;
    const std::int64_t pastLast = std::int64_t(pastLastRow) * columns;
    return {
        int(std::min<std::int64_t>(itemCount, first)),
        int(std::min<std::int64_t>(itemCount, pastLast)),
    };
}

}  // namespace rs::ui
