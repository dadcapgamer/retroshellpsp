#include "src/runtime/bounds.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main() {
    using namespace rs::bounds;
    assert(powerOfTwo(1));
    assert(powerOfTwo(4096));
    assert(!powerOfTwo(0));
    assert(!powerOfTwo(3));

    std::uint32_t sum = 0;
    assert(add(12, 30, sum) && sum == 42);
    assert(!add(std::numeric_limits<std::uint32_t>::max(), 1, sum));

    std::size_t product = 0;
    assert(multiply(480, 272, product) && product == 130560);
    assert(!multiply(std::numeric_limits<std::size_t>::max(), 2, product));

    assert(decompressionRatio(1, 200, 200));
    assert(!decompressionRatio(1, 201, 200));
    assert(!decompressionRatio(0, 1, 200));
    assert(decompressionRatio(0, 0, 200));

    assert(geometry(160, 144, 320, 2, 1024));
    assert(!geometry(160, 144, 319, 2, 1024));
    assert(!geometry(1025, 144, 2050, 2, 1024));
    assert(!geometry(0, 144, 0, 2, 1024));
    return 0;
}
