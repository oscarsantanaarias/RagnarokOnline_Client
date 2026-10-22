#include "ro/mode/mode_mgr.hpp"
#include "ro/mode/login_mode.hpp"
#include "ro/mode/game_mode.hpp"

namespace ro::mode {

CModeMgr& CModeMgr::instance() {
    static CModeMgr s_instance;
    return s_instance;
}

bool CModeMgr::switch_mode(ModeType next_mode, std::string_view initial_map) {
    if (m_active_mode) {
        m_active_mode->on_exit();
        m_active_mode.reset();
    }

    switch (next_mode) {
        case ModeType::Login:
            m_active_mode = std::make_unique<CLoginMode>();
            break;
        case ModeType::Game:
            m_active_mode = std::make_unique<CGameMode>();
            break;
        default:
            return false;
    }

    if (!m_active_mode->on_init(initial_map)) {
        m_active_mode.reset();
        return false;
    }

    return true;
}

bool CModeMgr::process() {
    if (m_quit_requested) {
        return false;
    }

    if (!m_active_mode) {
        return false;
    }

    // Fixed delta time tick (60 FPS = ~0.0166s)
    constexpr float delta_time = 1.0f / 60.0f;
    if (!m_active_mode->on_update(delta_time)) {
        return false;
    }

    m_active_mode->on_draw();
    return m_active_mode->on_run();
}

intptr_t CModeMgr::dispatch_msg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (m_active_mode) {
        return m_active_mode->on_msg(hwnd, msg, wparam, lparam);
    }
    return 0;
}

} // namespace ro::mode

