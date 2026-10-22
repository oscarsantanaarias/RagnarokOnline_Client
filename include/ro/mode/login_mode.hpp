#pragma once

#include "ro/mode/mode.hpp"
#include "ro/render/renderer.hpp"
#include "ro/resource/spr.hpp"
#include "ro/resource/act.hpp"
#include "ro/actor/actor.hpp"
#include "ro/world/gat.hpp"
#include "ro/world/gnd.hpp"
#include "ro/world/rsw.hpp"
#include "ro/world/rsm.hpp"
#include <memory>
#include <string>
#include <unordered_map>

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
    void load_world_scenery();
    void load_cursor_assets();
    void draw_cursor();
    void draw_fade_overlay();
    void process_screenshot();

    std::string m_map_name{"prontera.gat"};
    int32_t m_state{3};
    int32_t m_pending_state{-1};
    uint32_t m_frame_count{0};
    bool m_is_running{false};
    HWND m_hwnd{nullptr};

    // Fade-in overlay (sub_AD6740)
    bool m_fade_active{true};
    uint32_t m_fade_start_time{0};

    // Screenshot capture (sub_AD6A40)
    bool m_screenshot_requested{false};

    // Animated mouse cursor (sub_AD5520)
    std::shared_ptr<resource::SprResource> m_cursor_spr;
    std::shared_ptr<resource::ActResource> m_cursor_act;
    int32_t m_cursor_type{0};
    int32_t m_cursor_x{0};
    int32_t m_cursor_y{0};

    std::unique_ptr<render::SoftwareRenderer> m_renderer;
    std::unique_ptr<world::GatCollision> m_collision;
    std::unique_ptr<world::GndMap> m_gnd;
    std::unique_ptr<world::RswMap> m_rsw;
    std::unordered_map<std::string, std::shared_ptr<world::RsmModel>> m_model_cache;

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

    // Camera: Zoom & Rotation (Orbit Yaw and Pitch)
    float m_zoom{1.0f}; // 0.35x to 3.5x
    float m_camera_yaw{0.0f}; // Degrees: 0 = North (default RO angle), rotated via Right Click + drag
    float m_camera_pitch{45.0f}; // Degrees pitch
    bool m_is_rotating_cam{false};
    int32_t m_last_mouse_x{0};
    int32_t m_last_mouse_y{0};

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
