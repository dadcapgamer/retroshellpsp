/** Lightweight video-output health probe.
 *
 * Sampling a small grid makes black/static-frame telemetry cheap enough to
 * leave enabled on a 333 MHz PSP. This does not declare a game incompatible:
 * title screens and fades may legitimately be static or black. It makes the
 * condition explicit in the log so an emulation loop is never mistaken for
 * successful rendering.
 */
#pragma once

#include "core_api/rs_core_api.h"
#include "runtime/bounds.h"

#include <cstdint>
#include <cstring>

namespace rs {

class FrameProbe {
public:
    static constexpr std::uint32_t BLACK_WARNING_FRAMES = 120;
    static constexpr std::uint32_t STATIC_WARNING_FRAMES = 300;
    static constexpr std::uint32_t MISSING_WARNING_FRAMES = 120;

    void reset() { *this = FrameProbe{}; }

    void observe(const RSVideoFrame& frame) {
        const std::uint32_t bpp =
            frame.format == RS_PIXFMT_RGBA8888 ? 4u : 2u;
        if (!frame.pixels ||
            frame.format > RS_PIXFMT_RGBA8888 ||
            !bounds::geometry(frame.width, frame.height, frame.pitch, bpp,
                              1024)) {
            ++m_missingFrames;
            m_blackFrames = 0;
            m_staticFrames = 0;
            return;
        }
        m_missingFrames = 0;

        const auto* bytes = static_cast<const std::uint8_t*>(frame.pixels);
        std::uint32_t hash = 2166136261u;
        std::uint32_t nonBlack = 0;
        for (std::uint32_t gy = 0; gy < SAMPLE_AXIS; ++gy) {
            const std::uint32_t y =
                ((gy * 2u + 1u) * frame.height) / (SAMPLE_AXIS * 2u);
            const std::uint8_t* row = bytes + y * frame.pitch;
            for (std::uint32_t gx = 0; gx < SAMPLE_AXIS; ++gx) {
                const std::uint32_t x =
                    ((gx * 2u + 1u) * frame.width) / (SAMPLE_AXIS * 2u);
                std::uint32_t pixel = 0;
                std::memcpy(&pixel, row + x * bpp, bpp);
                const std::uint32_t colour =
                    frame.format == RS_PIXFMT_RGBA8888
                        ? pixel & 0x00FFFFFFu
                        : frame.format == RS_PIXFMT_RGBA5551
                              ? pixel & 0x00007FFFu
                              : pixel & 0x0000FFFFu;
                nonBlack += colour != 0;
                hash ^= pixel;
                hash *= 16777619u;
            }
        }
        hash ^= (std::uint32_t(frame.width) << 16) | frame.height;
        hash *= 16777619u;

        m_nonBlackSamples = nonBlack;
        m_blackFrames = nonBlack == 0 ? m_blackFrames + 1 : 0;
        m_staticFrames =
            m_haveHash && hash == m_lastHash ? m_staticFrames + 1 : 0;
        m_lastHash = hash;
        m_haveHash = true;
    }

    bool blackWarning() const {
        return m_blackFrames >= BLACK_WARNING_FRAMES;
    }
    bool staticWarning() const {
        return m_staticFrames >= STATIC_WARNING_FRAMES;
    }
    bool missingWarning() const {
        return m_missingFrames >= MISSING_WARNING_FRAMES;
    }
    std::uint32_t blackFrames() const { return m_blackFrames; }
    std::uint32_t staticFrames() const { return m_staticFrames; }
    std::uint32_t missingFrames() const { return m_missingFrames; }
    std::uint32_t nonBlackSamples() const { return m_nonBlackSamples; }

    const char* status() const {
        if (missingWarning()) return "missing";
        if (blackWarning()) return "black";
        if (staticWarning()) return "static";
        return "ok";
    }

private:
    static constexpr std::uint32_t SAMPLE_AXIS = 8;
    std::uint32_t m_lastHash = 0;
    std::uint32_t m_blackFrames = 0;
    std::uint32_t m_staticFrames = 0;
    std::uint32_t m_missingFrames = 0;
    std::uint32_t m_nonBlackSamples = 0;
    bool m_haveHash = false;
};

}  // namespace rs
