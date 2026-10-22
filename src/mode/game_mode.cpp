#include "ro/mode/game_mode.hpp"

namespace ro::mode {

CGameMode::CGameMode() = default;
CGameMode::~CGameMode() = default;

bool CGameMode::on_init(std::string_view map_name) {
    m_map_name = map_name;
    m_state = 0;
    m_is_running = true;
    return true;
}

bool CGameMode::on_exit() {
    m_is_running = false;
    return true;
}

bool CGameMode::on_run() {
    return m_is_running;
}

bool CGameMode::on_update(float /*delta_time*/) {
    if (!m_is_running) return false;
    // Process actor animations (ACT + SPR)
    // Dispatch network packets
    return true;
}

void CGameMode::on_draw() {
    // Render World map (.rsw, .gnd), Entities (.spr, .act), and UI
}

intptr_t CGameMode::on_msg(HWND /*hwnd*/, UINT /*msg*/, WPARAM /*wparam*/, LPARAM /*lparam*/) {
    return 0;
}

void CGameMode::on_change_state(int32_t new_state) {
    m_state = new_state;
}

} // namespace ro::mode

