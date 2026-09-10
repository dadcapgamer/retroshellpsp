#include "src/core_api/rs_pause_menu.h"

#include <cassert>
#include <cstring>

int main() {
    static const char* const expected[] = {
        "Resume", "Save State", "Load State", "Reset", "Aspect Ratio",
        "Filter", "Screenshot", "Emulator Settings", "Exit",
    };
    static_assert(sizeof(expected) / sizeof(expected[0]) ==
                  RS_PAUSE_ITEM_COUNT);
    for (int i = 0; i < RS_PAUSE_ITEM_COUNT; ++i)
        assert(std::strcmp(rs_pause_menu_label(RSPauseMenuItem(i)),
                           expected[i]) == 0);
    assert(std::strcmp(rs_pause_menu_label(RSPauseMenuItem(-1)), "") == 0);
    assert(std::strcmp(rs_pause_menu_label(RS_PAUSE_ITEM_COUNT), "") == 0);
    return 0;
}
