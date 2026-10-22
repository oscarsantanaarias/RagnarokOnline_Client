#pragma once

#include "ro/core/types.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <span>
#include <optional>

namespace ro::resource {

struct ActClip {
    int32_t x{0};
    int32_t y{0};
    int32_t spr_index{-1};
    int32_t mirror{0};      // 0 = normal, 1 = horizontal flip
    core::Color32 color{255, 255, 255, 255};
    float scale_x{1.0f};
    float scale_y{1.0f};
    int32_t rotation{0};
    int32_t spr_type{0};    // 0 = indexed sprite, 1 = RGBA sprite
    int32_t width{0};
    int32_t height{0};
};

struct ActAttachPoint {
    int32_t x{0};
    int32_t y{0};
    int32_t attr{0};
};

struct ActMotion {
    core::Rect range{};
    std::vector<ActClip> clips;
    int32_t sound_trigger{-1};
    std::vector<ActAttachPoint> attach_points;
};

struct ActAction {
    std::vector<ActMotion> motions;
};

class ActResource {
public:
    ActResource() = default;

    // Load .act directly from an in-memory byte buffer (e.g. read from GRF)
    [[nodiscard]] static std::optional<ActResource> load_from_memory(std::span<const uint8_t> buffer);

    [[nodiscard]] uint16_t version() const noexcept { return m_version; }
    [[nodiscard]] size_t action_count() const noexcept { return m_actions.size(); }
    [[nodiscard]] const std::vector<ActAction>& actions() const noexcept { return m_actions; }
    [[nodiscard]] const std::vector<std::string>& sound_files() const noexcept { return m_sound_files; }
    [[nodiscard]] const std::vector<float>& delay_factors() const noexcept { return m_delays; }

private:
    uint16_t m_version{0x0205};
    std::vector<ActAction> m_actions;
    std::vector<std::string> m_sound_files;
    std::vector<float> m_delays;
};

} // namespace ro::resource

