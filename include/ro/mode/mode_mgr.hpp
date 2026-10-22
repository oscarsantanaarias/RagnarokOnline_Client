#pragma once

#include "ro/mode/mode.hpp"
#include <memory>
#include <string_view>

namespace ro::mode {

class CModeMgr {
public:
    static CModeMgr& instance();

    bool switch_mode(ModeType next_mode, std::string_view initial_map = "login.rsw");
    bool process();
    intptr_t dispatch_msg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    [[nodiscard]] CMode* current_mode() const noexcept { return m_active_mode.get(); }
    [[nodiscard]] bool is_quitting() const noexcept { return m_quit_requested; }
    void request_quit() noexcept { m_quit_requested = true; }

private:
    CModeMgr() = default;
    ~CModeMgr() = default;

    std::unique_ptr<CMode> m_active_mode;
    ModeType m_pending_mode{ModeType::None};
    std::string m_pending_map;
    bool m_quit_requested{false};
};

} // namespace ro::mode

