#pragma once

#include "ro/core/types.hpp"
#include "ro/resource/bmp.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <span>
#include <optional>
#include <filesystem>

namespace ro::world {

struct GndTile {
    float u[4]{};
    float v[4]{};
    int16_t texture_index{-1};
    int16_t lightmap_index{-1};
    core::Color32 color{255, 255, 255, 255};
};

struct GndCell {
    float height[4]{};
    int32_t top_surface_id{-1};
    int32_t front_surface_id{-1};
    int32_t right_surface_id{-1};
};

class GndMap {
public:
    GndMap() = default;

    [[nodiscard]] static std::optional<GndMap> load_from_memory(
        std::span<const uint8_t> buffer,
        const std::filesystem::path& texture_base_dir = "data/texture"
    );

    [[nodiscard]] static std::optional<GndMap> load_from_file(
        const std::filesystem::path& gnd_path,
        const std::filesystem::path& texture_base_dir = "data/texture"
    );

    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] float scale() const noexcept { return m_scale; }

    [[nodiscard]] bool is_valid_cell(int32_t gx, int32_t gy) const noexcept;
    [[nodiscard]] const GndCell* get_cell(int32_t gx, int32_t gy) const noexcept;
    [[nodiscard]] const GndTile* get_tile(int32_t tile_id) const noexcept;

    // Samples texture color at world GAT cell coordinates (continuous world_x, world_y)
    [[nodiscard]] uint32_t sample_ground_color(float world_x, float world_y) const noexcept;

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    float m_scale{10.0f};

    std::vector<std::string> m_texture_names;
    std::vector<resource::BmpImage> m_textures;
    std::vector<GndTile> m_tiles;
    std::vector<GndCell> m_cells;
};

} // namespace ro::world
