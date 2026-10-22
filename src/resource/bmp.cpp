#include "ro/resource/bmp.hpp"
#include <fstream>
#include <cstring>

namespace ro::resource {

std::optional<BmpImage> BmpImage::load_from_memory(std::span<const uint8_t> buffer) {
    if (buffer.size() < 54) {
        return std::nullopt;
    }

    if (buffer[0] != 'B' || buffer[1] != 'M') {
        return std::nullopt;
    }

    uint32_t off_bits = 0;
    std::memcpy(&off_bits, &buffer[10], 4);

    int32_t raw_width = 0;
    int32_t raw_height = 0;
    uint16_t bit_count = 0;
    uint32_t compression = 0;
    uint32_t clr_used = 0;

    std::memcpy(&raw_width, &buffer[18], 4);
    std::memcpy(&raw_height, &buffer[22], 4);
    std::memcpy(&bit_count, &buffer[28], 2);
    std::memcpy(&compression, &buffer[30], 4);
    std::memcpy(&clr_used, &buffer[46], 4);

    if (raw_width <= 0 || raw_height == 0 || compression != 0) {
        return std::nullopt;
    }

    const uint32_t width = static_cast<uint32_t>(raw_width);
    const bool top_down = (raw_height < 0);
    const uint32_t height = static_cast<uint32_t>(top_down ? -raw_height : raw_height);

    BmpImage bmp{};
    bmp.m_width = width;
    bmp.m_height = height;
    bmp.m_pixels.resize(static_cast<size_t>(width) * height);

    if (bit_count == 8) {
        const size_t pal_count = (clr_used > 0 && clr_used <= 256) ? clr_used : 256;
        if (54 + pal_count * 4 > buffer.size()) {
            return std::nullopt;
        }

        uint32_t palette[256]{};
        for (size_t i = 0; i < pal_count; ++i) {
            const size_t p_off = 54 + i * 4;
            const uint8_t b = buffer[p_off];
            const uint8_t g = buffer[p_off + 1];
            const uint8_t r = buffer[p_off + 2];
            palette[i] = (static_cast<uint32_t>(r) << 16) |
                         (static_cast<uint32_t>(g) << 8)  |
                         static_cast<uint32_t>(b);
        }

        const size_t stride = ((static_cast<size_t>(width) + 3) / 4) * 4;
        if (off_bits + stride * height > buffer.size()) {
            return std::nullopt;
        }

        for (uint32_t y = 0; y < height; ++y) {
            const uint32_t out_y = top_down ? y : (height - 1 - y);
            const size_t in_row = off_bits + static_cast<size_t>(y) * stride;
            const size_t out_row = static_cast<size_t>(out_y) * width;

            for (uint32_t x = 0; x < width; ++x) {
                const uint8_t idx = buffer[in_row + x];
                bmp.m_pixels[out_row + x] = palette[idx];
            }
        }
        return bmp;
    } else if (bit_count == 24) {
        const size_t stride = ((static_cast<size_t>(width) * 3 + 3) / 4) * 4;
        if (off_bits + stride * height > buffer.size()) {
            return std::nullopt;
        }

        for (uint32_t y = 0; y < height; ++y) {
            const uint32_t out_y = top_down ? y : (height - 1 - y);
            const size_t in_row = off_bits + static_cast<size_t>(y) * stride;
            const size_t out_row = static_cast<size_t>(out_y) * width;

            for (uint32_t x = 0; x < width; ++x) {
                const size_t p = in_row + x * 3;
                const uint8_t b = buffer[p];
                const uint8_t g = buffer[p + 1];
                const uint8_t r = buffer[p + 2];
                bmp.m_pixels[out_row + x] = (static_cast<uint32_t>(r) << 16) |
                                            (static_cast<uint32_t>(g) << 8)  |
                                            static_cast<uint32_t>(b);
            }
        }
        return bmp;
    } else if (bit_count == 32) {
        const size_t stride = static_cast<size_t>(width) * 4;
        if (off_bits + stride * height > buffer.size()) {
            return std::nullopt;
        }

        for (uint32_t y = 0; y < height; ++y) {
            const uint32_t out_y = top_down ? y : (height - 1 - y);
            const size_t in_row = off_bits + static_cast<size_t>(y) * stride;
            const size_t out_row = static_cast<size_t>(out_y) * width;

            for (uint32_t x = 0; x < width; ++x) {
                const size_t p = in_row + x * 4;
                const uint8_t b = buffer[p];
                const uint8_t g = buffer[p + 1];
                const uint8_t r = buffer[p + 2];
                bmp.m_pixels[out_row + x] = (static_cast<uint32_t>(r) << 16) |
                                            (static_cast<uint32_t>(g) << 8)  |
                                            static_cast<uint32_t>(b);
            }
        }
        return bmp;
    }

    return std::nullopt;
}

std::optional<BmpImage> BmpImage::load_from_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }

    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return load_from_memory(buffer);
}

} // namespace ro::resource
