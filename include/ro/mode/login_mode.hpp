#pragma once

#include "ro/mode/mode.hpp"
#include "ro/render/renderer.hpp"
#include "ro/resource/spr.hpp"
#include "ro/resource/act.hpp"
#include "ro/actor/actor.hpp"
#include "ro/world/gat.hpp"
#include <memory>
#include <string>

namespace ro::mode {

class CLoginMode final : public CMode {
public:
    CLoginMode();
    ~CLoginMode() override;

    bool on_init(std::string_view map_name) override;
    bool on_exit() override;
    bool on_run() override;
    bool on_update(float delta_time) override;
    void on_draw() override;
    intptr_t on_msg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    void on_change_state(int32_t new_state) override;

    [[nodiscard]] ModeType type() const noexcept override { return ModeType::Login; }

private:
    void load_character_assets();

    std::string m_map_name{"login.rsw"};
    int32_t m_state{0};
    bool m_is_running{false};
    HWND m_hwnd{nullptr};

    std::unique_ptr<render::SoftwareRenderer> m_renderer;
    std::unique_ptr<world::GatCollision> m_collision;

    std::shared_ptr<resource::SprResource> m_body_spr;
    std::shared_ptr<resource::ActResource> m_body_act;

    std::shared_ptr<resource::SprResource> m_head_spr;
    std::shared_ptr<resource::ActResource> m_head_act;

    actor::Direction m_dir{actor::Direction::South};
    actor::ActionType m_action{actor::ActionType::Idle};

    size_t m_motion_idx{0};
    float m_anim_timer{0.0f};
    float m_frame_delay{0.20f};
    float m_elapsed_time{0.0f};

    // Camera & Zoom
    float m_zoom{1.0f}; // 0.4x (far out) to 3.0x (close up)

    // World & Navigation (Prontera GAT map cells)
    float m_world_cell_x{156.0f};
    float m_world_cell_y{180.0f};
    float m_target_cell_x{156.0f};
    float m_target_cell_y{180.0f};
    bool m_is_moving{false};
    float m_cell_move_speed{6.5f}; // cells per second

    float m_click_timer{0.0f};
    float m_click_screen_x{0.0f};
    float m_click_screen_y{0.0f};
};

} // namespace ro::mode
