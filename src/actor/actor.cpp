#include "ro/actor/actor.hpp"
#include <cmath>
#include <numbers>

namespace ro::actor {

void Actor::set_body(std::shared_ptr<resource::SprResource> spr, std::shared_ptr<resource::ActResource> act) {
    m_body_spr = std::move(spr);
    m_body_act = std::move(act);
}

void Actor::set_head(std::shared_ptr<resource::SprResource> spr, std::shared_ptr<resource::ActResource> act) {
    m_head_spr = std::move(spr);
    m_head_act = std::move(act);
}

void Actor::set_action(ActionType action) noexcept {
    if (m_action != action) {
        m_action = action;
        m_motion_index = 0;
        m_anim_time = 0.0f;
    }
}

void Actor::update(float delta_time, const world::GatCollision* collision) {
    // 1. Advance frame animation
    m_anim_time += delta_time;
    if (m_anim_time >= m_frame_delay) {
        m_anim_time = 0.0f;

        if (m_body_act && !m_body_act->actions().empty()) {
            const size_t act_idx = static_cast<size_t>(m_action) * 8 + static_cast<size_t>(m_direction);
            if (act_idx < m_body_act->actions().size()) {
                const auto& action = m_body_act->actions()[act_idx];
                if (!action.motions.empty()) {
                    m_motion_index = (m_motion_index + 1) % action.motions.size();
                }
            }
        }
    }

    // 2. Snap to ground altitude if collision is available
    if (collision) {
        m_z = collision->get_altitude(m_x, m_y);
    }
}

void Actor::move_towards(float target_x, float target_y, float speed) {
    const float dx = target_x - m_x;
    const float dy = target_y - m_y;
    const float dist = std::hypot(dx, dy);

    if (dist < 1.0f) {
        set_action(ActionType::Idle);
        return;
    }

    set_action(ActionType::Walk);

    // Calculate facing direction (8 sectors)
    const float angle = std::atan2(-dy, dx); // radians
    float deg = angle * (180.0f / std::numbers::pi_v<float>);
    if (deg < 0.0f) deg += 360.0f;

    // Map 360 degrees into 8 directions
    const int dir_sector = static_cast<int>((deg + 22.5f) / 45.0f) % 8;
    switch (dir_sector) {
        case 0: m_direction = Direction::East; break;
        case 1: m_direction = Direction::NorthEast; break;
        case 2: m_direction = Direction::North; break;
        case 3: m_direction = Direction::NorthWest; break;
        case 4: m_direction = Direction::West; break;
        case 5: m_direction = Direction::SouthWest; break;
        case 6: m_direction = Direction::South; break;
        case 7: m_direction = Direction::SouthEast; break;
    }

    const float move_step = std::min(speed, dist);
    m_x += (dx / dist) * move_step;
    m_y += (dy / dist) * move_step;
}

} // namespace ro::actor

