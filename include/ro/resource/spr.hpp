#pragma once

#include "ro/core/types.hpp"
#include <cstdint>
#include <vector>
#include <span>
#include <optional>

namespace ro::resource {

struct SprFrameIndexed {
    uint16_t width{0};
    uint16_t height{0};
    std::vector<uint8_t> pixels; // 8-bit palette indices
};

struct SprFrameRgba {
    uint16_t width{0};
    uint16_t height{0};
    std::vector<core::Color32> pixels; // 32-bit RGBA
};

class SprResource {
public:
    SprResource() = default;

    // Load .spr directly from an in-memory byte buffer (e.g. read from GRF)
    [[nodiscard]] static std::optional<SprResource> load_from_memory(std::span<const uint8_t> buffer);

    [[nodiscard]] uint16_t version() const noexcept { return m_version; }
    [[nodiscard]] size_t indexed_frame_count() const noexcept { return m_indexed_frames.size(); }
    [[nodiscard]] size_t rgba_frame_count() const noexcept { return m_rgba_frames.size(); }

    [[nodiscard]] const std::vector<SprFrameIndexed>& indexed_frames() const noexcept { return m_indexed_frames; }
    [[nodiscard]] const std::vector<SprFrameRgba>& rgba_frames() const noexcept { return m_rgba_frames; }
    [[nodiscard]] const core::Palette& palette() const noexcept { return m_palette; }

    // Converts an indexed frame to full 32-bit RGBA using the embedded palette
    [[nodiscard]] std::vector<core::Color32> decode_indexed_to_rgba(size_t frame_index) const;

private:
    uint16_t m_version{0x0201};
    std::vector<SprFrameIndexed> m_indexed_frames;
    std::vector<SprFrameRgba> m_rgba_frames;
    core::Palette m_palette{};
};

} // namespace ro::resource

