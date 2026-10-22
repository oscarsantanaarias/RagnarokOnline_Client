#include "ro/mode/mode_mgr.hpp"
#include "ro/mode/login_mode.hpp"
#include "ro/mode/game_mode.hpp"
#include <imm.h>
#include <chrono>
#include <utility>

namespace ro::mode {

CModeMgr& CModeMgr::instance() {
    static CModeMgr s_instance;
    return s_instance;
}

bool CModeMgr::switch_mode(ModeType next_mode, std::string_view initial_map) {
    if (next_mode != ModeType::Login && next_mode != ModeType::Game) {
        return false;
    }

    m_pending_mode = next_mode;
    m_pending_map = initial_map;
    return true;
}

bool CModeMgr::activate_pending_mode() {
    const ModeType next_mode = m_pending_mode;
    const std::string map_name = std::move(m_pending_map);
    m_pending_mode = ModeType::None;

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

    if (!m_active_mode->on_init(map_name)) {
        m_active_mode.reset();
        return false;
    }

    return true;
}

void CModeMgr::close_active_mode() {
    if (m_active_mode) {
        m_active_mode->on_exit();
        m_active_mode.reset();
    }
}

int CModeMgr::run(ModeType initial_mode, std::string_view initial_map) {
    m_quit_requested = false;
    if (!switch_mode(initial_mode, initial_map)) return 1;

    using clock = std::chrono::steady_clock;
    constexpr auto frame_time = std::chrono::microseconds(1000000 / 60);
    MSG msg{};

    while (!m_quit_requested && m_pending_mode != ModeType::None) {
        if (!activate_pending_mode()) return 1;

        auto last_frame = clock::now();
        auto next_frame = last_frame;
        while (!m_quit_requested && m_pending_mode == ModeType::None && m_active_mode->on_run()) {
            while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    m_quit_requested = true;
                    break;
                }
                if (msg.message == WM_KEYDOWN && msg.wParam == VK_PROCESSKEY) {
                    msg.wParam = ImmGetVirtualKey(msg.hwnd);
                }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            if (m_quit_requested) break;

            const auto now = clock::now();
            const float delta_time = std::chrono::duration<float>(now - last_frame).count();
            last_frame = now;
            if (!m_active_mode->on_update(delta_time)) break;
            m_active_mode->on_draw();

            next_frame += frame_time;
            const auto after_frame = clock::now();
            if (after_frame < next_frame) {
                const auto sleep_ms = std::chrono::duration_cast<std::chrono::milliseconds>(next_frame - after_frame).count();
                if (sleep_ms > 0) Sleep(static_cast<DWORD>(sleep_ms));
            } else {
                next_frame = after_frame;
            }
        }

        close_active_mode();
    }

    close_active_mode();
    return static_cast<int>(msg.wParam);
}

intptr_t CModeMgr::dispatch_msg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (m_active_mode) {
        return m_active_mode->on_msg(hwnd, msg, wparam, lparam);
    }
    return 0;
}

} // namespace ro::mode

