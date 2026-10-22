#pragma once

#include "ro/mode/mode.hpp"
#include <string>

namespace ro::mode {

class CGameMode final : public CMode {
public:
    CGameMode();
    ~CGameMode() override;

    bool on_init(std::string_view map_name) override;
    bool on_exit() override;
    bool on_run() override;
    bool on_update(float delta_time) override;
    void on_draw() override;
    intptr_t on_msg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    void on_change_state(int32_t new_state) override;

    [[nodiscard]] ModeType type() const noexcept override { return ModeType::Game; }
    [[nodiscard]] const std::string& current_map() const noexcept { return m_map_name; }

private:
    std::string m_map_name;
    int32_t m_state{0};
    bool m_is_running{false};
};

} // namespace ro::mode

