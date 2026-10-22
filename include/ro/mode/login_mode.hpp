#pragma once

#include "ro/mode/mode.hpp"
#include "ro/render/renderer.hpp"
#include "ro/resource/spr.hpp"
#include "ro/resource/act.hpp"
#include "ro/actor/actor.hpp"
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

    std::shared_ptr<resource::SprResource> m_body_spr;
    std::shared_ptr<resource::ActResource> m_body_act;

    std::shared_ptr<resource::SprResource> m_head_spr;
    std::shared_ptr<resource::ActResource> m_head_act;

    actor::Direction m_dir{actor::Direction::South};
    actor::ActionType m_action{actor::ActionType::Idle};

    size_t m_motion_idx{0};
    float m_anim_timer{0.0f};
    float m_frame_delay{0.22f};

    // Movement & Click-to-Move
    float m_pos_x{512.0f};
    float m_pos_y{460.0f};
    float m_target_x{512.0f};
    float m_target_y{460.0f};
    bool m_is_moving{false};
    float m_move_speed{150.0f}; // pixels per second

    float m_click_timer{0.0f};
    float m_click_x{0.0f};
    float m_click_y{0.0f};
};

} // namespace ro::mode
