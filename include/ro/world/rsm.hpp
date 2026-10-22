#pragma once

#include "ro/core/types.hpp"
#include "ro/resource/bmp.hpp"
#include "ro/world/rsw.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <optional>
#include <memory>
#include <filesystem>

namespace ro::world {

struct Vec2 {
    float u{0.0f};
    float v{0.0f};
};

struct RsmFace {
    uint16_t vertex_indices[3]{0, 0, 0};
    uint16_t texcoord_indices[3]{0, 0, 0};
    uint16_t texture_id{0};
    uint16_t padding{0};
    int32_t two_sided{0};
    int32_t smooth_group{0};
};

struct RsmNode {
    std::string name;
    std::string parent_name;
    std::vector<int32_t> texture_indices;
    float transform_matrix[12]{
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 0.0f
    };
    Vec3 offset_pos{};
    Vec3 pos{};
    float rot_angle{0.0f};
    Vec3 rot_axis{};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    std::vector<Vec3> vertices;
    std::vector<Vec2> texcoords;
    std::vector<RsmFace> faces;
};

class RsmModel {
public:
    RsmModel() = default;

    [[nodiscard]] static std::optional<RsmModel> load_from_memory(
        std::span<const uint8_t> buffer,
        const std::filesystem::path& texture_base_dir
    );

    [[nodiscard]] static std::optional<RsmModel> load_from_file(
        const std::filesystem::path& file_path,
        const std::filesystem::path& texture_base_dir
    );

    [[nodiscard]] uint16_t version() const noexcept { return m_version; }
    [[nodiscard]] const std::vector<std::string>& texture_names() const noexcept { return m_texture_names; }
    [[nodiscard]] const std::vector<std::shared_ptr<resource::BmpImage>>& textures() const noexcept { return m_textures; }
    [[nodiscard]] const std::vector<RsmNode>& nodes() const noexcept { return m_nodes; }
    [[nodiscard]] const std::string& main_node_name() const noexcept { return m_main_node; }

private:
    uint16_t m_version{0x0104};
    int32_t m_anim_len{0};
    int32_t m_shade_type{0};
    uint8_t m_alpha{255};

    std::vector<std::string> m_texture_names;
    std::vector<std::shared_ptr<resource::BmpImage>> m_textures;
    std::string m_main_node;
    std::vector<RsmNode> m_nodes;
};

} // namespace ro::world

