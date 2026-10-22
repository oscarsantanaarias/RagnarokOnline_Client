#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <string>
#include <optional>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace ro::resource {

class BmpImage {
public:
    BmpImage() = default;

    [[nodiscard]] static std::optional<BmpImage> load_from_memory(std::span<const uint8_t> buffer);
    [[nodiscard]] static std::optional<BmpImage> load_from_file(const std::filesystem::path& path);

    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] const std::vector<uint32_t>& pixels() const noexcept { return m_pixels; }
    [[nodiscard]] bool empty() const noexcept { return m_pixels.empty(); }

    // Samples texture at UV coordinates (0.0 to 1.0) with wrap-around
    [[nodiscard]] inline uint32_t sample(float u, float v) const noexcept {
        if (m_width == 0 || m_height == 0 || m_pixels.empty()) {
            return 0x00A89F8F;
        }

        // Texture repeat wrapping
        float u_wrap = u - std::floor(u);
        float v_wrap = v - std::floor(v);

        const uint32_t px = std::clamp(static_cast<uint32_t>(u_wrap * m_width), 0u, m_width - 1);
        const uint32_t py = std::clamp(static_cast<uint32_t>(v_wrap * m_height), 0u, m_height - 1);

        return m_pixels[py * m_width + px];
    }

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::vector<uint32_t> m_pixels; // 0x00RRGGBB format
};

} // namespace ro::resource
