#include "ro/render/renderer.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace ro::render {

SoftwareRenderer::SoftwareRenderer(uint32_t width, uint32_t height)
    : m_width(width), m_height(height) {
    m_pixels.resize(static_cast<size_t>(m_width) * m_height, 0x001A1A24);

    m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bmi.bmiHeader.biWidth = static_cast<LONG>(m_width);
    m_bmi.bmiHeader.biHeight = -static_cast<LONG>(m_height); // Top-down DIB
    m_bmi.bmiHeader.biPlanes = 1;
    m_bmi.bmiHeader.biBitCount = 32;
    m_bmi.bmiHeader.biCompression = BI_RGB;
}

void SoftwareRenderer::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_pixels.resize(static_cast<size_t>(m_width) * m_height, 0x001A1A24);
    m_bmi.bmiHeader.biWidth = static_cast<LONG>(m_width);
    m_bmi.bmiHeader.biHeight = -static_cast<LONG>(m_height);
}

void SoftwareRenderer::clear(uint32_t color_xrgb) {
    std::fill(m_pixels.begin(), m_pixels.end(), color_xrgb);
}

void SoftwareRenderer::draw_sprite(const resource::SprResource& spr, size_t frame_index, int32_t x, int32_t y,
                                   bool mirror, core::Color32 tint, float scale) {
    if (frame_index >= spr.indexed_frame_count() || scale <= 0.001f) {
        return;
    }

    const auto& frame = spr.indexed_frames()[frame_index];
    const auto& palette = spr.palette();

    const int32_t fw = frame.width;
    const int32_t fh = frame.height;

    const int32_t scaled_w = static_cast<int32_t>(fw * scale);
    const int32_t scaled_h = static_cast<int32_t>(fh * scale);

    if (scaled_w <= 0 || scaled_h <= 0) return;

    const uint32_t eff_tint_r = static_cast<uint32_t>(tint.r);
    const uint32_t eff_tint_g = static_cast<uint32_t>(tint.g);
    const uint32_t eff_tint_b = static_cast<uint32_t>(tint.b);

    const int32_t min_sy = std::max(0, -y);
    const int32_t max_sy = std::min(scaled_h, static_cast<int32_t>(m_height) - y);
    const int32_t min_sx = std::max(0, -x);
    const int32_t max_sx = std::min(scaled_w, static_cast<int32_t>(m_width) - x);

    for (int32_t sy = min_sy; sy < max_sy; ++sy) {
        const int32_t py = static_cast<int32_t>(sy / scale);
        if (py >= fh) continue;

        const int32_t dst_y = y + sy;
        const size_t row_dest = static_cast<size_t>(dst_y) * m_width;
        const size_t row_src = static_cast<size_t>(py) * fw;

        for (int32_t sx = min_sx; sx < max_sx; ++sx) {
            const int32_t px = static_cast<int32_t>(sx / scale);
            if (px >= fw) continue;

            const int32_t src_x = mirror ? (fw - 1 - px) : px;
            const uint8_t pal_idx = frame.pixels[row_src + src_x];

            if (pal_idx == 0) {
                continue; // Transparent color key
            }

            const auto& color = palette[pal_idx];
            const uint32_t alpha = static_cast<uint32_t>(tint.a);
            const size_t dest_idx = row_dest + (x + sx);

            if (alpha >= 250 && eff_tint_r >= 250 && eff_tint_g >= 250 && eff_tint_b >= 250) {
                m_pixels[dest_idx] = (static_cast<uint32_t>(color.r) << 16) |
                                     (static_cast<uint32_t>(color.g) << 8)  |
                                     static_cast<uint32_t>(color.b);
                continue;
            }

            const uint32_t dst_px = m_pixels[dest_idx];
            const uint32_t dst_r = (dst_px >> 16) & 0xFF;
            const uint32_t dst_g = (dst_px >> 8) & 0xFF;
            const uint32_t dst_b = dst_px & 0xFF;

            const uint32_t src_r = (static_cast<uint32_t>(color.r) * eff_tint_r) / 255;
            const uint32_t src_g = (static_cast<uint32_t>(color.g) * eff_tint_g) / 255;
            const uint32_t src_b = (static_cast<uint32_t>(color.b) * eff_tint_b) / 255;

            const uint32_t inv_a = 255 - alpha;
            const uint32_t out_r = (src_r * alpha + dst_r * inv_a) / 255;
            const uint32_t out_g = (src_g * alpha + dst_g * inv_a) / 255;
            const uint32_t out_b = (src_b * alpha + dst_b * inv_a) / 255;

            m_pixels[dest_idx] = (out_r << 16) | (out_g << 8) | out_b;
        }
    }
}

void SoftwareRenderer::draw_composite_actor(const resource::SprResource* body_spr, const resource::ActResource* body_act,
                                           const resource::SprResource* head_spr, const resource::ActResource* head_act,
                                           actor::ActionType action, actor::Direction dir, size_t motion_idx,
                                           int32_t center_x, int32_t center_y, float scale, float camera_yaw_deg) {
    if (!body_spr || !body_act || body_act->actions().empty() || scale <= 0.001f) return;

    // Adjust apparent facing direction by camera yaw (45 degrees per cardinal step)
    int yaw_steps = static_cast<int>(std::round(camera_yaw_deg / 45.0f)) % 8;
    if (yaw_steps < 0) yaw_steps += 8;
    const actor::Direction effective_dir = static_cast<actor::Direction>((static_cast<size_t>(dir) + 8 - yaw_steps) % 8);

    const size_t act_idx = static_cast<size_t>(action) * 8 + static_cast<size_t>(effective_dir);
    if (act_idx >= body_act->actions().size()) return;

    const auto& act = body_act->actions()[act_idx];
    if (act.motions.empty()) return;

    const auto& motion = act.motions[motion_idx % act.motions.size()];

    // 1. Draw Body clips
    for (const auto& clip : motion.clips) {
        if (clip.spr_index < 0 || static_cast<size_t>(clip.spr_index) >= body_spr->indexed_frame_count()) continue;

        const auto& frame = body_spr->indexed_frames()[clip.spr_index];
        const int32_t draw_x = center_x + static_cast<int32_t>((clip.x - frame.width / 2) * scale);
        const int32_t draw_y = center_y + static_cast<int32_t>((clip.y - frame.height / 2) * scale);

        draw_sprite(*body_spr, static_cast<size_t>(clip.spr_index), draw_x, draw_y, clip.mirror != 0, clip.color, scale);
    }

    // 2. Draw Head/Hair clips anchored to body attachment point
    if (head_spr && head_act && !head_act->actions().empty()) {
        int32_t head_offset_x = 0;
        int32_t head_offset_y = 0;

        if (!motion.attach_points.empty()) {
            head_offset_x = motion.attach_points[0].x;
            head_offset_y = motion.attach_points[0].y;
        }

        const size_t head_act_idx = static_cast<size_t>(effective_dir) % head_act->actions().size();
        const auto& head_motion_set = head_act->actions()[head_act_idx];
        if (!head_motion_set.motions.empty()) {
            const auto& head_motion = head_motion_set.motions[0];

            if (!head_motion.attach_points.empty()) {
                head_offset_x -= head_motion.attach_points[0].x;
                head_offset_y -= head_motion.attach_points[0].y;
            }

            const int32_t head_center_x = center_x + static_cast<int32_t>(head_offset_x * scale);
            const int32_t head_center_y = center_y + static_cast<int32_t>(head_offset_y * scale);

            for (const auto& clip : head_motion.clips) {
                if (clip.spr_index < 0 || static_cast<size_t>(clip.spr_index) >= head_spr->indexed_frame_count()) continue;
                const auto& frame = head_spr->indexed_frames()[clip.spr_index];
                const int32_t draw_x = head_center_x + static_cast<int32_t>((clip.x - frame.width / 2) * scale);
                const int32_t draw_y = head_center_y + static_cast<int32_t>((clip.y - frame.height / 2) * scale);
                draw_sprite(*head_spr, static_cast<size_t>(clip.spr_index), draw_x, draw_y, clip.mirror != 0, clip.color, scale);
            }
        }
    }
}

void SoftwareRenderer::draw_map_ground(
    const world::GatCollision& gat,
    const world::GndMap* gnd,
    float cam_x,
    float cam_y,
    float zoom,
    float yaw_deg,
    float time_sec
) {
    const float base_cell_size = 28.0f;
    const float cell_size = base_cell_size * zoom;
    if (cell_size < 2.0f) return;

    const float center_sx = m_width * 0.5f;
    const float center_sy = m_height * 0.5f;

    const float rad = yaw_deg * (3.1415926535f / 180.0f);
    const float cos_a = std::cos(rad);
    const float sin_a = std::sin(rad);

    const float half_diag = std::sqrt(center_sx * center_sx + center_sy * center_sy) / cell_size + 2.0f;
    const int32_t min_cx = std::max(0, static_cast<int32_t>(cam_x - half_diag));
    const int32_t max_cx = std::min(static_cast<int32_t>(gat.width()) - 1, static_cast<int32_t>(cam_x + half_diag));

    const int32_t min_cy = std::max(0, static_cast<int32_t>(cam_y - half_diag));
    const int32_t max_cy = std::min(static_cast<int32_t>(gat.height()) - 1, static_cast<int32_t>(cam_y + half_diag));

    const int32_t step = std::max(1, static_cast<int32_t>(cell_size));

    for (int32_t cy = min_cy; cy <= max_cy; ++cy) {
        for (int32_t cx = min_cx; cx <= max_cx; ++cx) {
            const float dx = static_cast<float>(cx) - cam_x;
            const float dy = static_cast<float>(cy) - cam_y;

            // Camera yaw rotation (around player center)
            const float rx = dx * cos_a - dy * sin_a;
            const float ry = dx * sin_a + dy * cos_a;

            const int32_t sx_start = static_cast<int32_t>(center_sx + rx * cell_size - cell_size * 0.5f);
            const int32_t sy_start = static_cast<int32_t>(center_sy - ry * cell_size - cell_size * 0.5f);
            const int32_t sx_end = sx_start + step;
            const int32_t sy_end = sy_start + step;

            if (sx_end < 0 || sx_start >= static_cast<int32_t>(m_width) ||
                sy_end < 0 || sy_start >= static_cast<int32_t>(m_height)) {
                continue;
            }

            const auto* cell = gat.get_cell(cx, cy);
            if (!cell) continue;

            // Sample ground texture from GND
            uint32_t tile_color = 0x00A39A8B;
            if (gnd) {
                tile_color = gnd->sample_ground_color(static_cast<float>(cx), static_cast<float>(cy));
            }

            switch (cell->type) {
                case world::CellType::Walkable: {
                    // Height slope relief shading
                    const float slope = (cell->top_left - cell->bottom_right) * 3.5f;
                    int32_t r = ((tile_color >> 16) & 0xFF) + static_cast<int32_t>(slope);
                    int32_t g = ((tile_color >> 8)  & 0xFF) + static_cast<int32_t>(slope);
                    int32_t b = (tile_color & 0xFF)        + static_cast<int32_t>(slope);
                    tile_color = (std::clamp(r, 0, 255) << 16) |
                                 (std::clamp(g, 0, 255) << 8)  |
                                 std::clamp(b, 0, 255);
                    break;
                }
                case world::CellType::NonWalkable: {
                    // Darken for non-walkable building or wall
                    int32_t r = (((tile_color >> 16) & 0xFF) * 5) / 10;
                    int32_t g = (((tile_color >> 8)  & 0xFF) * 5) / 10;
                    int32_t b = ((tile_color & 0xFF) * 5) / 10;
                    tile_color = (r << 16) | (g << 8) | b;
                    break;
                }
                case world::CellType::WalkableWater:
                case world::CellType::WalkableWater2: {
                    const float wave = std::sin(static_cast<float>(cx) * 0.5f + time_sec * 3.0f) * 15.0f;
                    int32_t b = std::clamp(static_cast<int32_t>(175 + wave), 0, 255);
                    int32_t g = std::clamp(static_cast<int32_t>(115 + wave * 0.6f), 0, 255);
                    tile_color = (0x20 << 16) | (g << 8) | b;
                    break;
                }
                case world::CellType::NonWalkableSnipe:
                case world::CellType::NonWalkableSnipe2: {
                    int32_t r = (((tile_color >> 16) & 0xFF) * 7) / 10;
                    int32_t g = (((tile_color >> 8)  & 0xFF) * 7) / 10;
                    int32_t b = ((tile_color & 0xFF) * 7) / 10;
                    tile_color = (r << 16) | (g << 8) | b;
                    break;
                }
            }

            const int32_t draw_y0 = std::clamp(sy_start, 0, static_cast<int32_t>(m_height));
            const int32_t draw_y1 = std::clamp(sy_end,   0, static_cast<int32_t>(m_height));
            const int32_t draw_x0 = std::clamp(sx_start, 0, static_cast<int32_t>(m_width));
            const int32_t draw_x1 = std::clamp(sx_end,   0, static_cast<int32_t>(m_width));

            for (int32_t dy = draw_y0; dy < draw_y1; ++dy) {
                const size_t row_idx = static_cast<size_t>(dy) * m_width;
                for (int32_t dx = draw_x0; dx < draw_x1; ++dx) {
                    if ((dx == sx_start || dy == sy_start) && cell_size >= 14.0f) {
                        m_pixels[row_idx + dx] = 0x00554E46; // Subtle grid bevel
                    } else {
                        m_pixels[row_idx + dx] = tile_color;
                    }
                }
            }
        }
    }
}

void SoftwareRenderer::present(HWND hwnd) {
    if (!hwnd) return;
    HDC hdc = GetDC(hwnd);
    if (!hdc) return;

    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int win_w = rc.right - rc.left;
    const int win_h = rc.bottom - rc.top;

    StretchDIBits(
        hdc,
        0, 0, win_w, win_h,
        0, 0, m_width, m_height,
        m_pixels.data(),
        &m_bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );

    ReleaseDC(hwnd, hdc);
}

} // namespace ro::render
