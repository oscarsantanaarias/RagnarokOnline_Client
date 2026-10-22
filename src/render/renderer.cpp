#include "ro/render/renderer.hpp"
#include <algorithm>
#include <cstring>

namespace ro::render {

SoftwareRenderer::SoftwareRenderer(uint32_t width, uint32_t height) {
    resize(width, height);
}

void SoftwareRenderer::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_pixels.assign(static_cast<size_t>(width) * height, 0x001A1A24);

    std::memset(&m_bmi, 0, sizeof(m_bmi));
    m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bmi.bmiHeader.biWidth = static_cast<LONG>(width);
    m_bmi.bmiHeader.biHeight = -static_cast<LONG>(height); // Top-down
    m_bmi.bmiHeader.biPlanes = 1;
    m_bmi.bmiHeader.biBitCount = 32;
    m_bmi.bmiHeader.biCompression = BI_RGB;
}

void SoftwareRenderer::clear(uint32_t color_xrgb) {
    std::fill(m_pixels.begin(), m_pixels.end(), color_xrgb);
}

void SoftwareRenderer::draw_sprite(const resource::SprResource& spr, size_t frame_index, int32_t x, int32_t y,
                                   bool mirror, core::Color32 tint) {
    if (frame_index >= spr.indexed_frame_count()) {
        return;
    }

    const auto& frame = spr.indexed_frames()[frame_index];
    const auto& palette = spr.palette();

    const int32_t fw = frame.width;
    const int32_t fh = frame.height;

    for (int32_t py = 0; py < fh; ++py) {
        const int32_t dest_y = y + py;
        if (dest_y < 0 || dest_y >= static_cast<int32_t>(m_height)) continue;

        for (int32_t px = 0; px < fw; ++px) {
            const int32_t src_x = mirror ? (fw - 1 - px) : px;
            const int32_t dest_x = x + px;
            if (dest_x < 0 || dest_x >= static_cast<int32_t>(m_width)) continue;

            const uint8_t pal_idx = frame.pixels[py * fw + src_x];
            if (pal_idx == 0) continue; // Color 0 is transparent key

            const auto color = palette[pal_idx];
            const uint32_t eff_tint_a = (tint.a == 0) ? 255 : tint.a;
            const uint32_t alpha = (static_cast<uint32_t>(color.a) * eff_tint_a) / 255;
            if (alpha == 0) continue;

            const size_t dest_idx = static_cast<size_t>(dest_y) * m_width + dest_x;
            const uint32_t dst_col = m_pixels[dest_idx];

            const uint32_t dst_r = (dst_col >> 16) & 0xFF;
            const uint32_t dst_g = (dst_col >> 8)  & 0xFF;
            const uint32_t dst_b = dst_col & 0xFF;

            const uint32_t eff_tint_r = (tint.r == 0 && tint.g == 0 && tint.b == 0) ? 255 : tint.r;
            const uint32_t eff_tint_g = (tint.r == 0 && tint.g == 0 && tint.b == 0) ? 255 : tint.g;
            const uint32_t eff_tint_b = (tint.r == 0 && tint.g == 0 && tint.b == 0) ? 255 : tint.b;

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
                                           int32_t center_x, int32_t center_y) {
    if (!body_spr || !body_act || body_act->actions().empty()) return;

    const size_t act_idx = static_cast<size_t>(action) * 8 + static_cast<size_t>(dir);
    if (act_idx >= body_act->actions().size()) return;

    const auto& act = body_act->actions()[act_idx];
    if (act.motions.empty()) return;

    const auto& motion = act.motions[motion_idx % act.motions.size()];

    // 1. Draw Body clips
    for (const auto& clip : motion.clips) {
        if (clip.spr_index < 0 || static_cast<size_t>(clip.spr_index) >= body_spr->indexed_frame_count()) continue;

        const auto& frame = body_spr->indexed_frames()[clip.spr_index];
        const int32_t draw_x = center_x + clip.x - (frame.width / 2);
        const int32_t draw_y = center_y + clip.y - (frame.height / 2);

        draw_sprite(*body_spr, static_cast<size_t>(clip.spr_index), draw_x, draw_y, clip.mirror != 0, clip.color);
    }

    // 2. Draw Head/Hair clips anchored to body attachment point
    if (head_spr && head_act && !head_act->actions().empty()) {
        int32_t head_offset_x = 0;
        int32_t head_offset_y = 0;

        if (!motion.attach_points.empty()) {
            head_offset_x = motion.attach_points[0].x;
            head_offset_y = motion.attach_points[0].y;
        }

        // Ragnarok head/hair ACTs contain the 8 directional head sprites.
        // We clamp/wrap dir to ensure it safely addresses the head action set.
        const size_t head_act_idx = static_cast<size_t>(dir) % head_act->actions().size();
        const auto& head_motion_set = head_act->actions()[head_act_idx];
        if (!head_motion_set.motions.empty()) {
            // Head uses motion 0 (neutral straight-ahead look vector).
            // Walk cycle bobbing is driven purely by the body's motion.attach_points[0].
            const auto& head_motion = head_motion_set.motions[0];

            // Snap neck anchor exactly as reversed from client sub_8105A0
            if (!head_motion.attach_points.empty()) {
                head_offset_x -= head_motion.attach_points[0].x;
                head_offset_y -= head_motion.attach_points[0].y;
            }

            const int32_t head_center_x = center_x + head_offset_x;
            const int32_t head_center_y = center_y + head_offset_y;

            for (const auto& clip : head_motion.clips) {
                if (clip.spr_index < 0 || static_cast<size_t>(clip.spr_index) >= head_spr->indexed_frame_count()) continue;
                const auto& frame = head_spr->indexed_frames()[clip.spr_index];
                const int32_t draw_x = head_center_x + clip.x - (frame.width / 2);
                const int32_t draw_y = head_center_y + clip.y - (frame.height / 2);
                draw_sprite(*head_spr, static_cast<size_t>(clip.spr_index), draw_x, draw_y, clip.mirror != 0, clip.color);
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
