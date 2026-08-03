#include "src/frontend/ui/grid_window.h"

#include <cassert>
#include <limits>

int main() {
    using rs::ui::visibleGridWindow;

    auto range = visibleGridWindow(0, 3, 2, 0.f);
    assert(range.first == 0 && range.pastLast == 0);

    range = visibleGridWindow(10000, 3, 2, 0.f);
    assert(range.first == 0 && range.pastLast == 6);

    /* Fractional scrolling includes the row entering at the bottom. */
    range = visibleGridWindow(10000, 3, 2, .25f);
    assert(range.first == 0 && range.pastLast == 9);

    range = visibleGridWindow(10000, 3, 2, 1200.5f);
    assert(range.first == 3600 && range.pastLast == 3609);
    assert(range.pastLast - range.first <= 9);

    range = visibleGridWindow(10000, 3, 2, 99999.f);
    assert(range.first <= range.pastLast);
    assert(range.pastLast == 10000);
    assert(range.pastLast - range.first <= 6);

    range = visibleGridWindow(
        12, 3, 2, std::numeric_limits<float>::quiet_NaN());
    assert(range.first == 0 && range.pastLast == 6);
    return 0;
}
