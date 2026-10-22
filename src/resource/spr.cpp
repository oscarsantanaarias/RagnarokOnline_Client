#include "ro/resource/spr.hpp"
#include <cstring>

namespace ro::resource {

std::optional<SprResource> SprResource::load_from_memory(std::span<const uint8_t> buffer) {
    if (buffer.size() < 8) {
        return std::nullopt;
    }

    if (buffer[0] != 'S' || buffer[1] != 'P') {
        return std::nullopt;
    }

    SprResource res{};
    std::memcpy(&res.m_version, &buffer[2], 2);

    uint16_t indexed_count = 0;
    std::memcpy(&indexed_count, &buffer[4], 2);

    uint16_t rgba_count = 0;
    if (res.m_version >= 0x0200 && buffer.size() >= 8) {
        std::memcpy(&rgba_count, &buffer[6], 2);
    }

    size_t cursor = (res.m_version >= 0x0200) ? 8 : 6;

    // 1. Read Indexed (Paletted) Frames
    res.m_indexed_frames.reserve(indexed_count);
    for (uint16_t i = 0; i < indexed_count; ++i) {
        if (cursor + 4 > buffer.size()) return std::nullopt;

        uint16_t w = 0, h = 0;
        std::memcpy(&w, &buffer[cursor], 2);
        std::memcpy(&h, &buffer[cursor + 2], 2);
        cursor += 4;

        if (w > 2048 || h > 2048) {
            return std::nullopt;
        }

        if (w > 2048 || h > 2048) {
            return std::nullopt;
        }

        SprFrameIndexed frame{};
        frame.width = w;
        frame.height = h;

        const size_t total_pixels = static_cast<size_t>(w) * h;
        frame.pixels.resize(total_pixels, 0);

        if (res.m_version >= 0x0201) {
            // RLE (Run-Length Encoded) decoding
            if (cursor + 2 > buffer.size()) return std::nullopt;
            uint16_t comp_size = 0;
            std::memcpy(&comp_size, &buffer[cursor], 2);
            cursor += 2;

            if (cursor + comp_size > buffer.size()) return std::nullopt;

            size_t out_idx = 0;
            const size_t end_cursor = cursor + comp_size;
            while (cursor < end_cursor && out_idx < total_pixels) {
                const uint8_t byte_val = buffer[cursor++];
                if (byte_val == 0) {
                    if (cursor >= end_cursor) break;
                    const uint8_t count = buffer[cursor++];
                    for (uint8_t c = 0; c < count && out_idx < total_pixels; ++c) {
                        frame.pixels[out_idx++] = 0;
                    }
                } else {
                    frame.pixels[out_idx++] = byte_val;
                }
            }
            cursor = end_cursor; // Advance past compressed frame payload
        } else {
            // Uncompressed 8-bit stream
            if (cursor + total_pixels > buffer.size()) return std::nullopt;
            std::memcpy(frame.pixels.data(), &buffer[cursor], total_pixels);
            cursor += total_pixels;
        }

        res.m_indexed_frames.push_back(std::move(frame));
    }

    // 2. Read 32-bit RGBA Frames (v2.0+)
    res.m_rgba_frames.reserve(rgba_count);
    for (uint16_t i = 0; i < rgba_count; ++i) {
        if (cursor + 4 > buffer.size()) return std::nullopt;

        uint16_t w = 0, h = 0;
        std::memcpy(&w, &buffer[cursor], 2);
        std::memcpy(&h, &buffer[cursor + 2], 2);
        cursor += 4;

        if (w > 2048 || h > 2048) {
            return std::nullopt;
        }

        SprFrameRgba frame{};
        frame.width = w;
        frame.height = h;

        const size_t pixel_count = static_cast<size_t>(w) * h;
        const size_t byte_count = pixel_count * 4;
        if (cursor + byte_count > buffer.size()) return std::nullopt;

        frame.pixels.resize(pixel_count);
        for (size_t p = 0; p < pixel_count; ++p) {
            const size_t byte_idx = cursor + p * 4;
            // Ragnarok RGBA format is stored as ABGR
            frame.pixels[p] = core::Color32{
                .r = buffer[byte_idx + 3],
                .g = buffer[byte_idx + 2],
                .b = buffer[byte_idx + 1],
                .a = buffer[byte_idx + 0]
            };
        }
        cursor += byte_count;
        res.m_rgba_frames.push_back(std::move(frame));
    }

    // 3. Read 1024-byte Palette (at EOF or next 1024 bytes)
    if (buffer.size() >= 1024) {
        const size_t pal_start = buffer.size() - 1024;
        for (size_t c = 0; c < 256; ++c) {
            const size_t p = pal_start + c * 4;
            res.m_palette[c] = core::Color32{
                .r = buffer[p],
                .g = buffer[p + 1],
                .b = buffer[p + 2],
                .a = (c == 0) ? uint8_t{0} : uint8_t{255} // Index 0 is transparent key
            };
        }
    }

    return res;
}

std::vector<core::Color32> SprResource::decode_indexed_to_rgba(size_t frame_index) const {
    if (frame_index >= m_indexed_frames.size()) {
        return {};
    }

    const auto& frame = m_indexed_frames[frame_index];
    std::vector<core::Color32> rgba(frame.pixels.size());

    for (size_t i = 0; i < frame.pixels.size(); ++i) {
        const uint8_t pal_idx = frame.pixels[i];
        rgba[i] = m_palette[pal_idx];
    }

    return rgba;
}

} // namespace ro::resource

