#include "ro/world/gat.hpp"
#include <cstring>
#include <algorithm>

namespace ro::world {

std::optional<GatCollision> GatCollision::load_from_memory(std::span<const uint8_t> buffer) {
    if (buffer.size() < 14) {
        return std::nullopt;
    }

    if (buffer[0] != 'G' || buffer[1] != 'R' || buffer[2] != 'A' || buffer[3] != 'T') {
        return std::nullopt;
    }

    GatCollision gat{};
    std::memcpy(&gat.m_width, &buffer[6], 4);
    std::memcpy(&gat.m_height, &buffer[10], 4);

    const size_t total_cells = static_cast<size_t>(gat.m_width) * gat.m_height;
    const size_t cell_data_size = total_cells * 20; // 4 floats (16 bytes) + 4 bytes type

    if (14 + cell_data_size > buffer.size()) {
        return std::nullopt;
    }

    gat.m_cells.resize(total_cells);
    size_t cursor = 14;

    for (size_t i = 0; i < total_cells; ++i) {
        auto& c = gat.m_cells[i];
        std::memcpy(&c.bottom_left, &buffer[cursor], 4);
        std::memcpy(&c.bottom_right, &buffer[cursor + 4], 4);
        std::memcpy(&c.top_left, &buffer[cursor + 8], 4);
        std::memcpy(&c.top_right, &buffer[cursor + 12], 4);

        uint32_t type_raw = 0;
        std::memcpy(&type_raw, &buffer[cursor + 16], 4);
        c.type = static_cast<CellType>(type_raw);

        cursor += 20;
    }

    return gat;
}

bool GatCollision::is_valid_cell(int32_t cx, int32_t cy) const noexcept {
    return cx >= 0 && cx < static_cast<int32_t>(m_width) &&
           cy >= 0 && cy < static_cast<int32_t>(m_height);
}

bool GatCollision::is_walkable(int32_t cx, int32_t cy) const noexcept {
    if (!is_valid_cell(cx, cy)) return false;
    const auto* cell = get_cell(cx, cy);
    if (!cell) return false;
    return cell->type == CellType::Walkable ||
           cell->type == CellType::WalkableWater ||
           cell->type == CellType::WalkableWater2;
}

const GatCell* GatCollision::get_cell(int32_t cx, int32_t cy) const noexcept {
    if (!is_valid_cell(cx, cy)) return nullptr;
    return &m_cells[static_cast<size_t>(cy) * m_width + cx];
}

float GatCollision::get_altitude(float world_x, float world_y) const noexcept {
    // Ragnarok coordinates: 1 cell = 5 world units (oriented with bottom-left origin)
    const float fx = world_x / 5.0f;
    const float fy = world_y / 5.0f;

    const int32_t cx = static_cast<int32_t>(fx);
    const int32_t cy = static_cast<int32_t>(fy);

    if (!is_valid_cell(cx, cy)) {
        return 0.0f;
    }

    const auto* cell = get_cell(cx, cy);
    if (!cell) return 0.0f;

    const float u = std::clamp(fx - static_cast<float>(cx), 0.0f, 1.0f);
    const float v = std::clamp(fy - static_cast<float>(cy), 0.0f, 1.0f);

    // Bilinear interpolation between the 4 corners of the cell
    const float h_bottom = (1.0f - u) * cell->bottom_left + u * cell->bottom_right;
    const float h_top    = (1.0f - u) * cell->top_left    + u * cell->top_right;

    return (1.0f - v) * h_bottom + v * h_top;
}

} // namespace ro::world

