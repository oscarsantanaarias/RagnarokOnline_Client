#include "ro/mode/login_mode.hpp"
#include "ro/resource/resource_mgr.hpp"
#include <iostream>
#include <fstream>
#include <format>
#include <cmath>

namespace ro::mode {

static actor::Direction direction_from_delta(float dx, float dy) {
    const float angle = std::atan2(dy, dx);
    float deg = angle * (180.0f / 3.14159265f);
    float ro_deg = deg - 90.0f;
    if (ro_deg < 0.0f) ro_deg += 360.0f;
    int dir_idx = static_cast<int>((ro_deg + 22.5f) / 45.0f) % 8;
    return static_cast<actor::Direction>(dir_idx);
}

CLoginMode::CLoginMode() = default;

CLoginMode::~CLoginMode() {
    on_exit();
}

void CLoginMode::load_character_assets() {
    std::ofstream dbg("debug_login.log", std::ios::app);
    dbg << "Entering load_character_assets" << std::endl;

    auto& res = resource::ResourceManager::instance();

    struct CharSet {
        std::string name;
        std::string body_spr;
        std::string body_act;
        std::string head_spr;
        std::string head_act;
    };

    const std::vector<CharSet> char_presets = {
        // Preset 0: Arch Mage Male with Hair 1
        {
            "Arch Mage (Male)",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\\xB3\xB2\\arch_mage_\xB3\xB2.spr",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\\xB3\xB2\\arch_mage_\xB3\xB2.act",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\\xB3\xB2\\1_\xB3\xB2.spr",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\\xB3\xB2\\1_\xB3\xB2.act"
        },
        // Preset 1: Arch Mage Female with Hair 1
        {
            "Arch Mage (Female)",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\\xBF\xA9\\arch_mage_\xBF\xA9.spr",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\\xBF\xA9\\arch_mage_\xBF\xA9.act",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\\xBF\xA9\\1_\xBF\xA9.spr",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\\xBF\xA9\\1_\xBF\xA9.act"
        },
        // Preset 2: Dragon Knight Male
        {
            "Dragon Knight (Male)",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\\xB3\xB2\\dragon_knight_\xB3\xB2.spr",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\\xB3\xB2\\dragon_knight_\xB3\xB2.act",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\\xB3\xB2\\1_\xB3\xB2.spr",
            "sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\\xB3\xB2\\1_\xB3\xB2.act"
        },
        // Preset 3: Ork Warrior (Monster test)
        {
            "Ork Warrior",
            "sprite\\ork_warrior.spr",
            "sprite\\ork_warrior.act",
            "",
            ""
        }
    };

    for (const auto& preset : char_presets) {
        dbg << "Testing preset: " << preset.name << std::endl;
        const bool has_spr = res.exists(preset.body_spr);
        const bool has_act = res.exists(preset.body_act);
        dbg << "  Body SPR exists: " << has_spr << ", Body ACT exists: " << has_act << std::endl;

        if (has_spr && has_act) {
            auto b_spr_bytes = res.read_file(preset.body_spr);
            auto b_act_bytes = res.read_file(preset.body_act);
            dbg << "  Body bytes read: " << b_spr_bytes.size() << " / " << b_act_bytes.size() << std::endl;

            auto b_spr = resource::SprResource::load_from_memory(b_spr_bytes);
            auto b_act = resource::ActResource::load_from_memory(b_act_bytes);
            if (b_spr && b_act) {
                m_body_spr = std::make_shared<resource::SprResource>(std::move(*b_spr));
                m_body_act = std::make_shared<resource::ActResource>(std::move(*b_act));
                dbg << "  Body loaded! Frames: " << m_body_spr->indexed_frame_count() 
                    << ", Actions: " << m_body_act->action_count() << std::endl;

                // Load head/hair if present
                dbg << "Checking hair: " << preset.head_spr << std::endl;
                if (!preset.head_spr.empty() && res.exists(preset.head_spr) && res.exists(preset.head_act)) {
                    dbg << "Reading hair bytes..." << std::endl;
                    auto h_spr_bytes = res.read_file(preset.head_spr);
                    auto h_act_bytes = res.read_file(preset.head_act);
                    dbg << "Hair bytes: " << h_spr_bytes.size() << " / " << h_act_bytes.size() << std::endl;
                    dbg << "Parsing hair SPR..." << std::endl;
                    auto h_spr = resource::SprResource::load_from_memory(h_spr_bytes);
                    dbg << "Hair SPR parsed: " << (h_spr.has_value()) << std::endl;

                    dbg << "Parsing hair ACT..." << std::endl;
                    auto h_act = resource::ActResource::load_from_memory(h_act_bytes);
                    dbg << "Hair ACT parsed: " << (h_act.has_value()) << std::endl;

                    if (h_spr && h_act) {
                        m_head_spr = std::make_shared<resource::SprResource>(std::move(*h_spr));
                        m_head_act = std::make_shared<resource::ActResource>(std::move(*h_act));
                        dbg << "Hair loaded! Frames: " << m_head_spr->indexed_frame_count() << std::endl;
                    }
                }
                dbg << "Preset loading finished successfully" << std::endl;
                return;
            }
        }
    }

    dbg << "Warning: No presets could be loaded" << std::endl;
}

bool CLoginMode::on_init(std::string_view map_name) {
    std::ofstream dbg("debug_login.log");
    dbg << "LoginMode on_init started" << std::endl;

    m_map_name = map_name.empty() ? "login.rsw" : std::string(map_name);
    m_state = 0;
    m_is_running = true;

    // Allocate 1024x768 32-bit software framebuffer
    dbg << "Creating SoftwareRenderer" << std::endl;
    m_renderer = std::make_unique<render::SoftwareRenderer>(1024, 768);

    dbg << "Loading character assets" << std::endl;
    dbg.close();

    load_character_assets();

    dbg.open("debug_login.log", std::ios::app);
    dbg << "LoginMode on_init completed" << std::endl;
    return true;
}

bool CLoginMode::on_exit() {
    m_is_running = false;
    return true;
}

bool CLoginMode::on_run() {
    return m_is_running;
}

bool CLoginMode::on_update(float delta_time) {
    if (!m_is_running) return false;

    // Click effect decay
    if (m_click_timer > 0.0f) {
        m_click_timer -= delta_time;
    }

    // Pathfinding / click-to-move interpolation
    if (m_is_moving) {
        const float dx = m_target_x - m_pos_x;
        const float dy = m_target_y - m_pos_y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= 6.0f) {
            // Arrived at destination
            m_pos_x = m_target_x;
            m_pos_y = m_target_y;
            m_is_moving = false;
            m_action = actor::ActionType::Idle;
            m_frame_delay = 0.22f; // Return to gentle breathing
            m_motion_idx = 0;
        } else {
            // Walking towards destination
            m_action = actor::ActionType::Walk;
            m_frame_delay = 0.09f; // Natural walk cadence (~90ms per step)
            m_dir = direction_from_delta(dx, dy);

            const float step = m_move_speed * delta_time;
            const float move_dist = (step > dist) ? dist : step;

            m_pos_x += (dx / dist) * move_dist;
            m_pos_y += (dy / dist) * move_dist;
        }
    }

    // Animation timer for active character sprite
    if (m_is_moving) {
        m_anim_timer += delta_time;
        if (m_anim_timer >= m_frame_delay) {
            m_anim_timer = 0.0f;
            m_motion_idx++;
        }
    } else {
        // In Idle, the character stands calmly at motion 0
        m_motion_idx = 0;
        m_anim_timer = 0.0f;
    }

    return true;
}

void CLoginMode::on_draw() {
    if (!m_renderer) return;

    if (!m_hwnd) {
        m_hwnd = FindWindowA("Ragnarok", nullptr);
    }

    // 1. Clear to dark fantasy slate background
    m_renderer->clear(0x001B1C24);

    // 2. Draw animated character at current position (m_pos_x, m_pos_y)
    if (m_body_spr && m_body_act) {
        m_renderer->draw_composite_actor(
            m_body_spr.get(),
            m_body_act.get(),
            m_head_spr.get(),
            m_head_act.get(),
            m_action,
            m_dir,
            m_motion_idx,
            static_cast<int32_t>(m_pos_x),
            static_cast<int32_t>(m_pos_y)
        );
    }

    // 3. Present backbuffer to Win32 GDI
    if (m_hwnd) {
        m_renderer->present(m_hwnd);

        // Overlay status text and click indicator via GDI
        HDC hdc = GetDC(m_hwnd);
        if (hdc) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(220, 220, 240));

            const std::string title = "Ragnarok Online Reconstructed [Modern C++20 | 1:1 Engine | 60 FPS]";
            TextOutA(hdc, 20, 20, title.c_str(), static_cast<int>(title.size()));

            const char* dir_names[] = { "South", "SouthWest", "West", "NorthWest", "North", "NorthEast", "East", "SouthEast" };
            const char* act_names[] = { "Idle", "Walk", "Sit", "PickUp", "ReadyFight", "Attack", "Hurt", "Freeze", "Dead", "Special" };

            const std::string status = std::format("Pos: ({:.0f}, {:.0f}) | Dir: {} | Action: {} [Click or hold mouse to walk!]",
                m_pos_x, m_pos_y,
                dir_names[static_cast<size_t>(m_dir) % 8],
                (static_cast<size_t>(m_action) < 10) ? act_names[static_cast<size_t>(m_action)] : "Unknown"
            );
            TextOutA(hdc, 20, 45, status.c_str(), static_cast<int>(status.size()));

            // Draw RO Green Click Ring Indicator
            if (m_click_timer > 0.0f) {
                HPEN greenPen = CreatePen(PS_SOLID, 2, RGB(80, 255, 100));
                HGDIOBJ oldPen = SelectObject(hdc, greenPen);
                HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                const int cx = static_cast<int>(m_click_x);
                const int cy = static_cast<int>(m_click_y);
                const int r = static_cast<int>(14.0f * (m_click_timer / 0.4f) + 4.0f);
                Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(greenPen);
            }

            ReleaseDC(m_hwnd, hdc);
        }
    }
}

intptr_t CLoginMode::on_msg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (hwnd) {
        m_hwnd = hwnd;
    }

    switch (msg) {
        case WM_LBUTTONDOWN: {
            m_target_x = static_cast<float>(LOWORD(lparam));
            m_target_y = static_cast<float>(HIWORD(lparam));
            m_is_moving = true;
            m_click_x = m_target_x;
            m_click_y = m_target_y;
            m_click_timer = 0.4f;
            return 1;
        }

        case WM_MOUSEMOVE: {
            if (wparam & MK_LBUTTON) {
                m_target_x = static_cast<float>(LOWORD(lparam));
                m_target_y = static_cast<float>(HIWORD(lparam));
                m_is_moving = true;
            }
            return 1;
        }

        case WM_KEYDOWN:
            switch (wparam) {
                case VK_LEFT:
                case 'A': {
                    m_target_x = std::max(60.0f, m_pos_x - 60.0f);
                    m_target_y = m_pos_y;
                    m_is_moving = true;
                    return 1;
                }
                case VK_RIGHT:
                case 'D': {
                    m_target_x = std::min(960.0f, m_pos_x + 60.0f);
                    m_target_y = m_pos_y;
                    m_is_moving = true;
                    return 1;
                }
                case VK_UP:
                case 'W': {
                    m_target_y = std::max(80.0f, m_pos_y - 60.0f);
                    m_target_x = m_pos_x;
                    m_is_moving = true;
                    return 1;
                }
                case VK_DOWN:
                case 'S': {
                    m_target_y = std::min(700.0f, m_pos_y + 60.0f);
                    m_target_x = m_pos_x;
                    m_is_moving = true;
                    return 1;
                }
                case VK_SPACE: {
                    // Cycle action (Idle -> Walk -> Sit -> Attack -> etc.)
                    const auto a = (static_cast<int>(m_action) + 1) % 7;
                    m_action = static_cast<actor::ActionType>(a);
                    m_is_moving = false;
                    m_motion_idx = 0;
                    return 1;
                }
                case VK_TAB: {
                    // Reload assets
                    load_character_assets();
                    return 1;
                }
                case VK_ESCAPE:
                    m_is_running = false;
                    PostQuitMessage(0);
                    return 1;
            }
            break;
    }
    return 0;
}

void CLoginMode::on_change_state(int32_t new_state) {
    m_state = new_state;
}

} // namespace ro::mode
