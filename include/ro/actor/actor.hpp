#pragma once

#include "ro/core/types.hpp"
#include "ro/resource/spr.hpp"
#include "ro/resource/act.hpp"
#include "ro/world/gat.hpp"
#include <memory>
#include <string>

namespace ro::actor {

enum class Direction : uint8_t {
    South     = 0,
    SouthWest = 1,
    West      = 2,
    NorthWest = 3,
    North     = 4,
    NorthEast = 5,
    East      = 6,
    SouthEast = 7
};

enum class ActionType : uint32_t {
    Idle   = 0,
    Walk   = 1,
    Sit    = 2,
    PickUp = 3,
    Ready  = 4,
    Attack = 5,
    Hurt   = 6,
    Die    = 7
};

struct RenderLayer {
    const resource::SprResource* spr{nullptr};
    const resource::ActResource* act{nullptr};
};

class Actor {
public:
    Actor() = default;

    void set_body(std::shared_ptr<resource::SprResource> spr, std::shared_ptr<resource::ActResource> act);
    void set_head(std::shared_ptr<resource::SprResource> spr, std::shared_ptr<resource::ActResource> act);

    void set_position(float x, float y, float z) noexcept { m_x = x; m_y = y; m_z = z; }
    void set_direction(Direction dir) noexcept { m_direction = dir; }
    void set_action(ActionType action) noexcept;

    void update(float delta_time, const world::GatCollision* collision = nullptr);
    void move_towards(float target_x, float target_y, float speed = 35.0f);

    [[nodiscard]] float x() const noexcept { return m_x; }
    [[nodiscard]] float y() const noexcept { return m_y; }
    [[nodiscard]] float z() const noexcept { return m_z; }
    [[nodiscard]] Direction direction() const noexcept { return m_direction; }
    [[nodiscard]] ActionType action() const noexcept { return m_action; }

    [[nodiscard]] size_t current_motion_index() const noexcept { return m_motion_index; }

private:
    float m_x{0.0f};
    float m_y{0.0f};
    float m_z{0.0f};

    Direction m_direction{Direction::South};
    ActionType m_action{ActionType::Idle};

    size_t m_motion_index{0};
    float m_anim_time{0.0f};
    float m_frame_delay{0.15f};

    std::shared_ptr<resource::SprResource> m_body_spr;
    std::shared_ptr<resource::ActResource> m_body_act;

    std::shared_ptr<resource::SprResource> m_head_spr;
    std::shared_ptr<resource::ActResource> m_head_act;
};

} // namespace ro::actor

