#include "src/frontend/database/natural_order.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

int main() {
    std::vector<std::string> names = {
        "Game 100", "game 10", "Game 2", "alpha", "Beta", "Game 01",
    };
    std::sort(names.begin(), names.end(), rs::db::naturalNameLess);
    const std::vector<std::string> expected = {
        "alpha", "Beta", "Game 01", "Game 2", "game 10", "Game 100",
    };
    assert(names == expected);
    return 0;
}
