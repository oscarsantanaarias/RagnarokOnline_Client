#include "ro/world/gnd.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ro::world {

static std::filesystem::path to_native_path(std::string_view ansi_path) {
    if (ansi_path.empty()) return {};
    std::wstring wide;
    wide.reserve(ansi_path.size());
    for (char c : ansi_path) {
        wide.push_back(static_cast<wchar_t>(static_cast<uint8_t>(c)));
    }
    return std::filesystem::path(wide);
}

std::optional<GndMap> GndMap::load_from_memory(
    std::span<const uint8_t> buffer,
    const std::filesystem::path& texture_base_dir
) {
    if (buffer.size() < 26) {
        return std::nullopt;
    }

    if (buffer[0] != 'G' || buffer[1] != 'R' || buffer[2] != 'G' || buffer[3] != 'N') {
        return std::nullopt;
    }

    GndMap gnd{};
    std::memcpy(&gnd.m_width, &buffer[6], 4);
    std::memcpy(&gnd.m_height, &buffer[10], 4);
    std::memcpy(&gnd.m_scale, &buffer[14], 4);

    uint32_t tex_count = 0;
    uint32_t tex_name_len = 0;
    std::memcpy(&tex_count, &buffer[18], 4);
    std::memcpy(&tex_name_len, &buffer[22], 4);

    if (tex_name_len == 0 || tex_name_len > 260) {
        return std::nullopt;
    }

    size_t cursor = 26;
    if (cursor + static_cast<size_t>(tex_count) * tex_name_len > buffer.size()) {
        return std::nullopt;
    }

    gnd.m_texture_names.resize(tex_count);
    gnd.m_textures.resize(tex_count);

    for (size_t i = 0; i < tex_count; ++i) {
        const char* name_ptr = reinterpret_cast<const char*>(&buffer[cursor]);
        const size_t len = strnlen(name_ptr, tex_name_len);
        std::string raw_name(name_ptr, len);
        gnd.m_texture_names[i] = raw_name;
        cursor += tex_name_len;

        try {
            const auto native_sub = to_native_path(raw_name);
            const std::filesystem::path candidates[] = {
                texture_base_dir / native_sub,
                std::filesystem::path(L"data/texture") / native_sub,
                std::filesystem::path(L"data") / native_sub,
                native_sub
            };

            for (const auto& cand : candidates) {
                std::error_code ec;
                if (std::filesystem::exists(cand, ec)) {
                    auto bmp = resource::BmpImage::load_from_file(cand);
                    if (bmp) {
                        gnd.m_textures[i] = std::move(*bmp);
                        break;
                    }
                }
            }
        } catch (...) {
            // Gracefully continue if an individual texture file has an IO issue
        }
    }

    // Skip lightmaps header & data (each lightmap is 8x8x4 = 256 bytes)
    if (cursor + 16 > buffer.size()) {
        return std::nullopt;
    }

    uint32_t lm_count = 0;
    std::memcpy(&lm_count, &buffer[cursor], 4);
    cursor += 16; // 4 int32 fields (count, width, height, grid_size)

    const size_t lm_data_size = static_cast<size_t>(lm_count) * 256;
    if (cursor + lm_data_size > buffer.size()) {
        return std::nullopt;
    }
    cursor += lm_data_size;

    // Surface Tiles
    if (cursor + 4 > buffer.size()) {
        return std::nullopt;
    }

    uint32_t tile_count = 0;
    std::memcpy(&tile_count, &buffer[cursor], 4);
    cursor += 4;

    const size_t tile_data_size = static_cast<size_t>(tile_count) * 40;
    if (cursor + tile_data_size > buffer.size()) {
        return std::nullopt;
    }

    gnd.m_tiles.resize(tile_count);
    for (size_t i = 0; i < tile_count; ++i) {
        auto& t = gnd.m_tiles[i];
        std::memcpy(t.u, &buffer[cursor], 16);
        std::memcpy(t.v, &buffer[cursor + 16], 16);
        std::memcpy(&t.texture_index, &buffer[cursor + 32], 2);
        std::memcpy(&t.lightmap_index, &buffer[cursor + 34], 2);
        std::memcpy(&t.color, &buffer[cursor + 36], 4);
        cursor += 40;
    }

    // Grid Cells
    const size_t total_cells = static_cast<size_t>(gnd.m_width) * gnd.m_height;
    const size_t cell_data_size = total_cells * 28;

    if (cursor + cell_data_size > buffer.size()) {
        return std::nullopt;
    }

    gnd.m_cells.resize(total_cells);
    for (size_t i = 0; i < total_cells; ++i) {
        auto& c = gnd.m_cells[i];
        std::memcpy(c.height, &buffer[cursor], 16);
        std::memcpy(&c.top_surface_id, &buffer[cursor + 16], 4);
        std::memcpy(&c.front_surface_id, &buffer[cursor + 20], 4);
        std::memcpy(&c.right_surface_id, &buffer[cursor + 24], 4);
        cursor += 28;
    }

    return gnd;
}

std::optional<GndMap> GndMap::load_from_file(
    const std::filesystem::path& gnd_path,
    const std::filesystem::path& texture_base_dir
) {
    std::ifstream file(gnd_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }

    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);

    return load_from_memory(buffer, texture_base_dir);
}

bool GndMap::is_valid_cell(int32_t gx, int32_t gy) const noexcept {
    return gx >= 0 && gx < static_cast<int32_t>(m_width) &&
           gy >= 0 && gy < static_cast<int32_t>(m_height);
}

const GndCell* GndMap::get_cell(int32_t gx, int32_t gy) const noexcept {
    if (!is_valid_cell(gx, gy)) return nullptr;
    return &m_cells[static_cast<size_t>(gy) * m_width + static_cast<size_t>(gx)];
}

const GndTile* GndMap::get_tile(int32_t tile_id) const noexcept {
    if (tile_id < 0 || static_cast<size_t>(tile_id) >= m_tiles.size()) return nullptr;
    return &m_tiles[static_cast<size_t>(tile_id)];
}

uint32_t GndMap::sample_ground_color(float world_x, float world_y) const noexcept {
    // 1 GND tile covers exactly 2x2 GAT cells
    const float gx_f = world_x * 0.5f;
    const float gy_f = world_y * 0.5f;

    const int32_t gx = static_cast<int32_t>(std::floor(gx_f));
    const int32_t gy = static_cast<int32_t>(std::floor(gy_f));

    const auto* cell = get_cell(gx, gy);
    if (!cell || cell->top_surface_id < 0) {
        return 0x00A89F8F;
    }

    const auto* tile = get_tile(cell->top_surface_id);
    if (!tile || tile->texture_index < 0 || static_cast<size_t>(tile->texture_index) >= m_textures.size()) {
        return 0x00A89F8F;
    }

    const auto& tex = m_textures[tile->texture_index];
    if (tex.empty()) {
        return 0x00A89F8F;
    }

    // Bilinear UV interpolation across tile quad
    const float alpha = std::clamp(gx_f - static_cast<float>(gx), 0.0f, 1.0f);
    // In RO coordinates, Y increases upwards, so bottom-to-top is inverted in UV space
    const float beta = std::clamp(1.0f - (gy_f - static_cast<float>(gy)), 0.0f, 1.0f);

    const float u_top = tile->u[0] + alpha * (tile->u[1] - tile->u[0]);
    const float u_bot = tile->u[2] + alpha * (tile->u[3] - tile->u[2]);
    const float u = u_top + beta * (u_bot - u_top);

    const float v_left = tile->v[0] + beta * (tile->v[2] - tile->v[0]);
    const float v_right = tile->v[1] + beta * (tile->v[3] - tile->v[1]);
    const float v = v_left + alpha * (v_right - v_left);

    return tex.sample(u, v);
}

} // namespace ro::world
