#include "ro/mode/login_mode.hpp"
#include "ro/resource/resource_mgr.hpp"
#include <iostream>
#include <fstream>
#include <format>
#include <cmath>
#include <algorithm>

namespace ro::mode {

static actor::Direction direction_from_world_delta(float dx, float dy) {
    // dy > 0 is North (+Y), dy < 0 is South (-Y)
    // dx > 0 is East (+X), dx < 0 is West (-X)
    float deg = std::atan2(dy, dx) * (180.0f / 3.1415926535f);
    // In Ragnarok, South (270 deg) is index 0, West (180 deg) is index 2,
    // North (90 deg) is index 4, East (0 deg) is index 6.
    float ro_deg = 270.0f - deg;
    while (ro_deg < 0.0f) ro_deg += 360.0f;
    while (ro_deg >= 360.0f) ro_deg -= 360.0f;
    int dir = static_cast<int>((ro_deg + 22.5f) / 45.0f) % 8;
    return static_cast<actor::Direction>(dir);
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

    m_map_name = map_name.empty() ? "prontera.gat" : std::string(map_name);
    m_state = 0;
    m_is_running = true;

    // Allocate 1024x768 32-bit software framebuffer
    dbg << "Creating SoftwareRenderer (1024x768)" << std::endl;
    m_renderer = std::make_unique<render::SoftwareRenderer>(1024, 768);

    // 1. Load ground collision map (Prontera GAT)
    auto& res = resource::ResourceManager::instance();
    auto gat_bytes = res.read_file("data\\prontera.gat");
    if (gat_bytes.empty()) {
        gat_bytes = res.read_file("prontera.gat");
    }

    if (!gat_bytes.empty()) {
        dbg << "Read GAT bytes via ResourceManager: " << gat_bytes.size() << std::endl;
        auto gat = world::GatCollision::load_from_memory(gat_bytes);
        if (gat) {
            m_collision = std::make_unique<world::GatCollision>(std::move(*gat));
            dbg << "GatCollision loaded: " << m_collision->width() << "x" << m_collision->height() << std::endl;
        }
    }

    // Direct filesystem fallback if not found in ResourceManager
    if (!m_collision) {
        dbg << "Attempting fallback read for prontera.gat directly..." << std::endl;
        std::ifstream f("data/prontera.gat", std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
            f.open("prontera.gat", std::ios::binary | std::ios::ate);
        }
        if (f.is_open()) {
            const auto size = f.tellg();
            f.seekg(0, std::ios::beg);
            std::vector<uint8_t> buf(size);
            f.read(reinterpret_cast<char*>(buf.data()), size);
            auto gat = world::GatCollision::load_from_memory(buf);
            if (gat) {
                m_collision = std::make_unique<world::GatCollision>(std::move(*gat));
                dbg << "GatCollision loaded via fallback: " << m_collision->width() << "x" << m_collision->height() << std::endl;
            }
        }
    }

    // 2. Load ground textures and mesh (Prontera GND)
    dbg << "Loading Prontera ground mesh and textures (prontera.gnd)..." << std::endl;
    auto gnd_opt = world::GndMap::load_from_file("data/prontera.gnd", "data/texture");
    if (!gnd_opt) {
        gnd_opt = world::GndMap::load_from_file("prontera.gnd", "data/texture");
    }

    if (gnd_opt) {
        m_gnd = std::make_unique<world::GndMap>(std::move(*gnd_opt));
        dbg << "GndMap loaded successfully! Dimensions: " << m_gnd->width() << "x" << m_gnd->height() << std::endl;
    } else {
        dbg << "Notice: prontera.gnd could not be opened directly, trying memory buffer..." << std::endl;
        auto gnd_bytes = res.read_file("data\\prontera.gnd");
        if (gnd_bytes.empty()) gnd_bytes = res.read_file("prontera.gnd");
        if (!gnd_bytes.empty()) {
            auto mem_gnd = world::GndMap::load_from_memory(gnd_bytes, "data/texture");
            if (mem_gnd) {
                m_gnd = std::make_unique<world::GndMap>(std::move(*mem_gnd));
                dbg << "GndMap loaded via ResourceManager! Dimensions: " << m_gnd->width() << "x" << m_gnd->height() << std::endl;
            }
        }
    }

    // Default character spawn at Prontera central plaza
    m_world_cell_x = 156.0f;
    m_world_cell_y = 180.0f;
    m_target_cell_x = 156.0f;
    m_target_cell_y = 180.0f;
    m_zoom = 1.0f;
    m_camera_yaw = 0.0f;
    m_camera_pitch = 45.0f;

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

    m_elapsed_time += delta_time;

    // Click effect decay
    if (m_click_timer > 0.0f) {
        m_click_timer -= delta_time;
    }

    // Movement & collision resolution in world cell space
    if (m_is_moving) {
        const float dx = m_target_cell_x - m_world_cell_x;
        const float dy = m_target_cell_y - m_world_cell_y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= 0.12f) {
            // Reached destination cell
            m_world_cell_x = m_target_cell_x;
            m_world_cell_y = m_target_cell_y;
            m_is_moving = false;
            m_action = actor::ActionType::Idle;
            m_frame_delay = 0.20f;
            m_motion_idx = 0;
        } else {
            // Walking towards target in world coordinates
            m_action = actor::ActionType::Walk;
            m_frame_delay = 0.09f;
            m_dir = direction_from_world_delta(dx, dy);

            const float step = m_cell_move_speed * delta_time;
            const float move_dist = (step > dist) ? dist : step;

            const float next_x = m_world_cell_x + (dx / dist) * move_dist;
            const float next_y = m_world_cell_y + (dy / dist) * move_dist;

            // Collision check against GAT cells
            if (m_collision) {
                const int32_t check_cx = static_cast<int32_t>(std::round(next_x));
                const int32_t check_cy = static_cast<int32_t>(std::round(next_y));

                if (m_collision->is_walkable(check_cx, check_cy)) {
                    m_world_cell_x = next_x;
                    m_world_cell_y = next_y;
                } else {
                    // Try sliding along X axis
                    const int32_t curr_cy = static_cast<int32_t>(std::round(m_world_cell_y));
                    if (m_collision->is_walkable(check_cx, curr_cy)) {
                        m_world_cell_x = next_x;
                    } else {
                        // Try sliding along Y axis
                        const int32_t curr_cx = static_cast<int32_t>(std::round(m_world_cell_x));
                        if (m_collision->is_walkable(curr_cx, check_cy)) {
                            m_world_cell_y = next_y;
                        } else {
                            // Blocked by obstacle
                            m_is_moving = false;
                            m_action = actor::ActionType::Idle;
                            m_motion_idx = 0;
                        }
                    }
                }
            } else {
                m_world_cell_x = next_x;
                m_world_cell_y = next_y;
            }
        }
    }

    // Sprite action animation updates
    if (m_is_moving) {
        m_anim_timer += delta_time;
        if (m_anim_timer >= m_frame_delay) {
            m_anim_timer = 0.0f;
            m_motion_idx++;
        }
    } else {
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

    // 1. Clear background
    m_renderer->clear(0x001B1C24);

    // 2. Draw textured ground map centered on player world cell with camera rotation
    if (m_collision) {
        m_renderer->draw_map_ground(
            *m_collision,
            m_gnd.get(),
            m_world_cell_x,
            m_world_cell_y,
            m_zoom,
            m_camera_yaw,
            m_elapsed_time
        );
    }

    // 3. Draw animated character at screen center with zoom scaling and camera rotation
    const int32_t center_x = static_cast<int32_t>(m_renderer->width() / 2);
    const int32_t center_y = static_cast<int32_t>(m_renderer->height() / 2);

    if (m_body_spr && m_body_act) {
        m_renderer->draw_composite_actor(
            m_body_spr.get(),
            m_body_act.get(),
            m_head_spr.get(),
            m_head_act.get(),
            m_action,
            m_dir,
            m_motion_idx,
            center_x,
            center_y,
            m_zoom,
            m_camera_yaw
        );
    }

    // 4. Present backbuffer to Win32 GDI
    if (m_hwnd) {
        m_renderer->present(m_hwnd);

        // Overlay status text and click indicator via GDI
        HDC hdc = GetDC(m_hwnd);
        if (hdc) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));

            const std::string title = "Ragnarok Online Reconstructed [Modern C++20 | 1:1 Engine | 60 FPS]";
            TextOutA(hdc, 20, 20, title.c_str(), static_cast<int>(title.size()));

            const char* dir_names[] = { "South", "SouthWest", "West", "NorthWest", "North", "NorthEast", "East", "SouthEast" };
            const char* act_names[] = { "Idle", "Walk", "Sit", "PickUp", "ReadyFight", "Attack", "Hurt", "Freeze", "Dead", "Special" };

            const std::string status = std::format(
                "Map: Prontera | Cell: ({:.1f}, {:.1f}) | Zoom: {:.2f}x | Cam Yaw: {:.0f}\xC2\xB0 | Dir: {} | Action: {}",
                m_world_cell_x, m_world_cell_y, m_zoom, m_camera_yaw,
                dir_names[static_cast<size_t>(m_dir) % 8],
                (static_cast<size_t>(m_action) < 10) ? act_names[static_cast<size_t>(m_action)] : "Unknown"
            );
            TextOutA(hdc, 20, 45, status.c_str(), static_cast<int>(status.size()));

            SetTextColor(hdc, RGB(180, 225, 255));
            const std::string hint = "Controls: Right-Click + Drag = Rotate Camera | Left-Click = Walk | Wheel = Zoom | Space = Action";
            TextOutA(hdc, 20, 70, hint.c_str(), static_cast<int>(hint.size()));

            // Draw RO Green Click Ring Indicator in rotated camera space
            if (m_click_timer > 0.0f) {
                const float base_cell_size = 28.0f;
                const float cell_size = base_cell_size * m_zoom;

                const float dx = m_target_cell_x - m_world_cell_x;
                const float dy = m_target_cell_y - m_world_cell_y;

                const float rad = m_camera_yaw * (3.1415926535f / 180.0f);
                const float rx = dx * std::cos(rad) - dy * std::sin(rad);
                const float ry = dx * std::sin(rad) + dy * std::cos(rad);

                const float click_sx = center_x + rx * cell_size;
                const float click_sy = center_y - ry * cell_size;

                HPEN greenPen = CreatePen(PS_SOLID, 2, RGB(80, 255, 100));
                HGDIOBJ oldPen = SelectObject(hdc, greenPen);
                HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                const int cx = static_cast<int>(click_sx);
                const int cy = static_cast<int>(click_sy);
                const int r = static_cast<int>((16.0f * (m_click_timer / 0.4f) + 4.0f) * m_zoom);
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
        case WM_MOUSEWHEEL: {
            const short delta = GET_WHEEL_DELTA_WPARAM(wparam);
            if (delta > 0) {
                m_zoom = std::clamp(m_zoom * 1.15f, 0.35f, 3.5f);
            } else if (delta < 0) {
                m_zoom = std::clamp(m_zoom * 0.87f, 0.35f, 3.5f);
            }
            return 1;
        }

        // Camera Rotation (Right Mouse Button Drag)
        case WM_RBUTTONDOWN: {
            m_is_rotating_cam = true;
            m_last_mouse_x = static_cast<int16_t>(LOWORD(lparam));
            m_last_mouse_y = static_cast<int16_t>(HIWORD(lparam));
            SetCapture(hwnd);
            return 1;
        }

        case WM_RBUTTONUP: {
            m_is_rotating_cam = false;
            ReleaseCapture();
            return 1;
        }

        case WM_RBUTTONDBLCLK: {
            // Reset camera orientation to default North view
            m_camera_yaw = 0.0f;
            m_camera_pitch = 45.0f;
            return 1;
        }

        // Navigation (Left Mouse Button Click)
        case WM_LBUTTONDOWN: {
            const float sx = static_cast<float>(static_cast<int16_t>(LOWORD(lparam)));
            const float sy = static_cast<float>(static_cast<int16_t>(HIWORD(lparam)));
            const float center_sx = m_renderer ? (m_renderer->width() * 0.5f) : 512.0f;
            const float center_sy = m_renderer ? (m_renderer->height() * 0.5f) : 384.0f;
            const float base_cell_size = 28.0f;
            const float cell_size = base_cell_size * m_zoom;

            // Screen delta relative to screen center
            const float rx = (sx - center_sx) / cell_size;
            const float ry = (center_sy - sy) / cell_size;

            // Unrotate by -yaw angle to get continuous world cell coordinates
            const float rad = -m_camera_yaw * (3.1415926535f / 180.0f);
            const float wx = rx * std::cos(rad) - ry * std::sin(rad);
            const float wy = rx * std::sin(rad) + ry * std::cos(rad);

            m_target_cell_x = m_world_cell_x + wx;
            m_target_cell_y = m_world_cell_y + wy;
            m_is_moving = true;
            m_click_screen_x = sx;
            m_click_screen_y = sy;
            m_click_timer = 0.4f;
            return 1;
        }

        case WM_MOUSEMOVE: {
            const int16_t mouse_x = static_cast<int16_t>(LOWORD(lparam));
            const int16_t mouse_y = static_cast<int16_t>(HIWORD(lparam));

            if (m_is_rotating_cam) {
                const float delta_x = static_cast<float>(mouse_x - m_last_mouse_x);
                const float delta_y = static_cast<float>(mouse_y - m_last_mouse_y);

                m_camera_yaw += delta_x * 0.45f;
                while (m_camera_yaw < 0.0f) m_camera_yaw += 360.0f;
                while (m_camera_yaw >= 360.0f) m_camera_yaw -= 360.0f;

                m_camera_pitch = std::clamp(m_camera_pitch + delta_y * 0.35f, 20.0f, 85.0f);

                m_last_mouse_x = mouse_x;
                m_last_mouse_y = mouse_y;
                return 1;
            }

            if (wparam & MK_LBUTTON) {
                const float sx = static_cast<float>(mouse_x);
                const float sy = static_cast<float>(mouse_y);
                const float center_sx = m_renderer ? (m_renderer->width() * 0.5f) : 512.0f;
                const float center_sy = m_renderer ? (m_renderer->height() * 0.5f) : 384.0f;
                const float base_cell_size = 28.0f;
                const float cell_size = base_cell_size * m_zoom;

                const float rx = (sx - center_sx) / cell_size;
                const float ry = (center_sy - sy) / cell_size;

                const float rad = -m_camera_yaw * (3.1415926535f / 180.0f);
                const float wx = rx * std::cos(rad) - ry * std::sin(rad);
                const float wy = rx * std::sin(rad) + ry * std::cos(rad);

                m_target_cell_x = m_world_cell_x + wx;
                m_target_cell_y = m_world_cell_y + wy;
                m_is_moving = true;
                return 1;
            }
            break;
        }

        case WM_KEYDOWN:
            switch (wparam) {
                // Zoom hotkeys
                case VK_OEM_PLUS:
                case VK_ADD:
                case VK_PRIOR: { // Page Up
                    m_zoom = std::clamp(m_zoom * 1.15f, 0.35f, 3.5f);
                    return 1;
                }
                case VK_OEM_MINUS:
                case VK_SUBTRACT:
                case VK_NEXT: { // Page Down
                    m_zoom = std::clamp(m_zoom * 0.87f, 0.35f, 3.5f);
                    return 1;
                }
                case '0':
                case VK_NUMPAD0:
                case 'R': {
                    m_zoom = 1.0f;
                    m_camera_yaw = 0.0f;
                    return 1;
                }

                // Keyboard camera rotation
                case 'Q': {
                    m_camera_yaw -= 15.0f;
                    if (m_camera_yaw < 0.0f) m_camera_yaw += 360.0f;
                    return 1;
                }
                case 'E': {
                    m_camera_yaw += 15.0f;
                    if (m_camera_yaw >= 360.0f) m_camera_yaw -= 360.0f;
                    return 1;
                }

                // WASD relative to current camera yaw
                case VK_LEFT:
                case 'A': {
                    const float rad = -m_camera_yaw * (3.1415926535f / 180.0f);
                    m_target_cell_x = m_world_cell_x - 3.0f * std::cos(rad);
                    m_target_cell_y = m_world_cell_y - 3.0f * std::sin(rad);
                    m_is_moving = true;
                    return 1;
                }
                case VK_RIGHT:
                case 'D': {
                    const float rad = -m_camera_yaw * (3.1415926535f / 180.0f);
                    m_target_cell_x = m_world_cell_x + 3.0f * std::cos(rad);
                    m_target_cell_y = m_world_cell_y + 3.0f * std::sin(rad);
                    m_is_moving = true;
                    return 1;
                }
                case VK_UP:
                case 'W': {
                    const float rad = -m_camera_yaw * (3.1415926535f / 180.0f);
                    m_target_cell_x = m_world_cell_x - 3.0f * std::sin(rad);
                    m_target_cell_y = m_world_cell_y + 3.0f * std::cos(rad);
                    m_is_moving = true;
                    return 1;
                }
                case VK_DOWN:
                case 'S': {
                    const float rad = -m_camera_yaw * (3.1415926535f / 180.0f);
                    m_target_cell_x = m_world_cell_x + 3.0f * std::sin(rad);
                    m_target_cell_y = m_world_cell_y - 3.0f * std::cos(rad);
                    m_is_moving = true;
                    return 1;
                }
                case VK_SPACE: {
                    const auto a = (static_cast<int>(m_action) + 1) % 7;
                    m_action = static_cast<actor::ActionType>(a);
                    m_is_moving = false;
                    m_motion_idx = 0;
                    return 1;
                }
                case VK_TAB: {
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
