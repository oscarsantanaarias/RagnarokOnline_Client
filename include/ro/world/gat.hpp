#pragma once

#include "ro/core/types.hpp"
#include <cstdint>
#include <vector>
#include <span>
#include <optional>

namespace ro::world {

enum class CellType : uint32_t {
    Walkable          = 0,
    NonWalkable       = 1,
    WalkableWater     = 2,
    WalkableWater2    = 3,
    NonWalkableSnipe  = 4,
    NonWalkableSnipe2 = 5,
};

struct GatCell {
    float bottom_left{0.0f};
    float bottom_right{0.0f};
    float top_left{0.0f};
    float top_right{0.0f};
    CellType type{CellType::Walkable};
};

class GatCollision {
public:
    GatCollision() = default;

    [[nodiscard]] static std::optional<GatCollision> load_from_memory(std::span<const uint8_t> buffer);

    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }

    [[nodiscard]] bool is_valid_cell(int32_t cx, int32_t cy) const noexcept;
    [[nodiscard]] bool is_walkable(int32_t cx, int32_t cy) const noexcept;
    [[nodiscard]] const GatCell* get_cell(int32_t cx, int32_t cy) const noexcept;

    // Calculates precise ground height at any continuous (x, y) world coordinate using bilinear interpolation
    [[nodiscard]] float get_altitude(float world_x, float world_y) const noexcept;

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::vector<GatCell> m_cells;
};

} // namespace ro::world

