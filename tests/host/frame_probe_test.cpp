#include "src/runtime/frame_probe.h"

#include <array>
#include <cassert>
#include <cstdint>

int main() {
    using rs::FrameProbe;

    std::array<std::uint16_t, 16 * 16> pixels{};
    RSVideoFrame frame{
        pixels.data(), 16, 16, 32, RS_PIXFMT_RGB565, 0, 16, 0, 1};
    FrameProbe probe;

    for (std::uint32_t i = 0; i < FrameProbe::BLACK_WARNING_FRAMES; ++i)
        probe.observe(frame);
    assert(probe.blackWarning());
    assert(!probe.staticWarning());
    assert(probe.nonBlackSamples() == 0);

    pixels[9 * 16 + 9] = 0xFFFF;
    probe.observe(frame);
    assert(!probe.blackWarning());
    assert(probe.nonBlackSamples() > 0);

    for (std::uint32_t i = 0; i <= FrameProbe::STATIC_WARNING_FRAMES; ++i)
        probe.observe(frame);
    assert(probe.staticWarning());

    pixels[9 * 16 + 9] ^= 1;
    probe.observe(frame);
    assert(!probe.staticWarning());

    RSVideoFrame missing{};
    for (std::uint32_t i = 0; i < FrameProbe::MISSING_WARNING_FRAMES; ++i)
        probe.observe(missing);
    assert(probe.missingWarning());

    RSVideoFrame badPitch{
        pixels.data(), 16, 16, 2, RS_PIXFMT_RGB565, 0, 16, 0, 1};
    probe.reset();
    probe.observe(badPitch);
    assert(probe.missingFrames() == 1);
    return 0;
}
