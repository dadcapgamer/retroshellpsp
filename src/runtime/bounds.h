#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace rs::bounds {

constexpr bool powerOfTwo(std::uint32_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

constexpr bool add(std::uint32_t left, std::uint32_t right,
                   std::uint32_t& result) {
    if (right > std::numeric_limits<std::uint32_t>::max() - left)
        return false;
    result = left + right;
    return true;
}

constexpr bool multiply(std::size_t left, std::size_t right,
                        std::size_t& result) {
    if (left && right > std::numeric_limits<std::size_t>::max() / left)
        return false;
    result = left * right;
    return true;
}

constexpr bool decompressionRatio(std::uint64_t compressed,
                                  std::uint64_t expanded,
                                  std::uint64_t maximum) {
    if (!expanded) return true;
    if (!compressed || !maximum) return false;
    return expanded / compressed <= maximum;
}

constexpr bool geometry(std::uint32_t width, std::uint32_t height,
                        std::uint32_t pitch, std::uint32_t bytesPerPixel,
                        std::uint32_t maximumDimension) {
    if (!width || !height || !bytesPerPixel ||
        width > maximumDimension || height > maximumDimension)
        return false;
    std::size_t row = 0;
    std::size_t frame = 0;
    return multiply(width, bytesPerPixel, row) && pitch >= row &&
           multiply(pitch, height, frame) && frame <= UINT32_MAX;
}

}  // namespace rs::bounds
