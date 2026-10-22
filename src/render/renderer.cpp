#include "ro/render/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace ro::render {

SoftwareRenderer::SoftwareRenderer(uint32_t width, uint32_t height)
    : m_width(width), m_height(height), m_pixels(width * height, 0x001A1A24),
      m_depth_buffer(width * height, 1e9f) {
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
    m_pixels.assign(width * height, 0x001A1A24);
    m_depth_buffer.assign(width * height, 1e9f);
    m_bmi.bmiHeader.biWidth = static_cast<LONG>(m_width);
    m_bmi.bmiHeader.biHeight = -static_cast<LONG>(m_height);
}

void SoftwareRenderer::clear(uint32_t color_xrgb) {
    std::fill(m_pixels.begin(), m_pixels.end(), color_xrgb);
    std::fill(m_depth_buffer.begin(), m_depth_buffer.end(), 1e9f);
}

void SoftwareRenderer::draw_sprite(
    const resource::SprResource& spr,
    size_t frame_index,
    int32_t x,
    int32_t y,
    bool mirror,
    core::Color32 tint,
    float scale
) {
    if (frame_index >= spr.indexed_frame_count()) {
        return;
    }

    const auto& frame = spr.indexed_frames()[frame_index];
    if (frame.pixels.empty()) return;

    const auto& pal = spr.palette();
    const int32_t src_w = static_cast<int32_t>(frame.width);
    const int32_t src_h = static_cast<int32_t>(frame.height);

    const int32_t dst_w = static_cast<int32_t>(std::round(src_w * scale));
    const int32_t dst_h = static_cast<int32_t>(std::round(src_h * scale));
    if (dst_w <= 0 || dst_h <= 0) return;

    // In Ragnarok Online, ACT clip positions (x, y) specify the CENTER of the sprite frame
    const int32_t top_left_x = x - (dst_w / 2);
    const int32_t top_left_y = y - (dst_h / 2);

    const int32_t min_dx = std::max<int32_t>(0, top_left_x);
    const int32_t max_dx = std::min<int32_t>(static_cast<int32_t>(m_width), top_left_x + dst_w);
    const int32_t min_dy = std::max<int32_t>(0, top_left_y);
    const int32_t max_dy = std::min<int32_t>(static_cast<int32_t>(m_height), top_left_y + dst_h);

    if (min_dx >= max_dx || min_dy >= max_dy) return;

    for (int32_t dy = min_dy; dy < max_dy; ++dy) {
        const int32_t sy = std::clamp<int32_t>(static_cast<int32_t>((dy - top_left_y) / scale), 0, src_h - 1);
        const size_t dst_row = static_cast<size_t>(dy) * m_width;
        const size_t src_row = static_cast<size_t>(sy) * src_w;

        for (int32_t dx = min_dx; dx < max_dx; ++dx) {
            int32_t sx = std::clamp<int32_t>(static_cast<int32_t>((dx - top_left_x) / scale), 0, src_w - 1);
            if (mirror) {
                sx = src_w - 1 - sx;
            }

            const uint8_t pal_idx = frame.pixels[src_row + sx];
            if (pal_idx == 0) continue; // Transparency

            const auto& c = pal[pal_idx];
            const uint32_t tr = (static_cast<uint32_t>(c.r) * tint.r) / 255;
            const uint32_t tg = (static_cast<uint32_t>(c.g) * tint.g) / 255;
            const uint32_t tb = (static_cast<uint32_t>(c.b) * tint.b) / 255;

            // Character depth at screen center is slightly in front of ground (-0.01f)
            if (-0.01f <= m_depth_buffer[dst_row + dx]) {
                m_pixels[dst_row + dx] = (tr << 16) | (tg << 8) | tb;
                m_depth_buffer[dst_row + dx] = -0.01f;
            }
        }
    }
}

void SoftwareRenderer::draw_composite_actor(
    const resource::SprResource* body_spr,
    const resource::ActResource* body_act,
    const resource::SprResource* head_spr,
    const resource::ActResource* head_act,
    actor::ActionType action,
    actor::Direction dir,
    size_t motion_idx,
    int32_t center_x,
    int32_t center_y,
    float scale,
    float camera_yaw_deg
) {
    if (!body_spr || !body_act) return;

    const int32_t yaw_offset = static_cast<int32_t>(std::round(camera_yaw_deg / 45.0f));
    const auto effective_dir = static_cast<actor::Direction>((static_cast<int32_t>(dir) + yaw_offset + 8) % 8);

    const size_t action_idx = static_cast<size_t>(action) * 8 + static_cast<size_t>(effective_dir);
    if (action_idx >= body_act->action_count()) return;

    const auto& body_action_clip = body_act->actions()[action_idx];
    if (body_action_clip.motions.empty()) return;

    const auto& body_motion = body_action_clip.motions[motion_idx % body_action_clip.motions.size()];

    for (const auto& clip : body_motion.clips) {
        if (clip.spr_index < 0) continue;

        const int32_t draw_x = center_x + static_cast<int32_t>(std::round(clip.x * scale));
        const int32_t draw_y = center_y + static_cast<int32_t>(std::round(clip.y * scale));
        const float clip_scale = scale * clip.scale_x;

        draw_sprite(
            *body_spr,
            static_cast<size_t>(clip.spr_index),
            draw_x,
            draw_y,
            clip.mirror != 0,
            clip.color,
            clip_scale
        );
    }

    if (head_spr && head_act) {
        const size_t head_action_idx = static_cast<size_t>(action) * 8 + static_cast<size_t>(effective_dir);
        if (head_action_idx < head_act->action_count()) {
            const auto& head_action_clip = head_act->actions()[head_action_idx];
            if (!head_action_clip.motions.empty()) {
                const auto& head_motion = head_action_clip.motions[motion_idx % head_action_clip.motions.size()];

                // In official Ragnarok Online, when both body and head have attach points,
                // the head is offset by (body_attach[0] - head_attach[0]) to align the neck seamlessly.
                int32_t offset_x = 0;
                int32_t offset_y = 0;
                if (!body_motion.attach_points.empty() && !head_motion.attach_points.empty()) {
                    offset_x = body_motion.attach_points[0].x - head_motion.attach_points[0].x;
                    offset_y = body_motion.attach_points[0].y - head_motion.attach_points[0].y;
                }

                for (const auto& clip : head_motion.clips) {
                    if (clip.spr_index < 0) continue;

                    const int32_t draw_x = center_x + static_cast<int32_t>(std::round((clip.x + offset_x) * scale));
                    const int32_t draw_y = center_y + static_cast<int32_t>(std::round((clip.y + offset_y) * scale));
                    const float clip_scale = scale * clip.scale_x;

                    draw_sprite(
                        *head_spr,
                        static_cast<size_t>(clip.spr_index),
                        draw_x,
                        draw_y,
                        clip.mirror != 0,
                        clip.color,
                        clip_scale
                    );
                }
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
    float pitch_deg,
    float time_sec
) {
    const float base_cell_size = 28.0f;
    const float cell_size = base_cell_size * zoom;
    if (cell_size <= 0.001f) return;

    const float center_sx = m_width * 0.5f;
    const float center_sy = m_height * 0.5f;

    const float rad_yaw = -yaw_deg * (std::numbers::pi_v<float> / 180.0f);
    const float cos_yaw = std::cos(rad_yaw);
    const float sin_yaw = std::sin(rad_yaw);

    const float rad_pitch = std::clamp(pitch_deg, 20.0f, 85.0f) * (std::numbers::pi_v<float> / 180.0f);
    const float sin_pitch = std::sin(rad_pitch);

    const float map_w = static_cast<float>(gat.width());
    const float map_h = static_cast<float>(gat.height());

    const float dWx = cos_yaw / cell_size;
    const float dWy = -sin_yaw / cell_size;

    for (uint32_t y = 0; y < m_height; ++y) {
        const float sy_rel = (center_sy - static_cast<float>(y)) / (cell_size * sin_pitch);
        const float rx_start = -center_sx / cell_size;

        const float wx_start = cam_x + rx_start * cos_yaw + sy_rel * sin_yaw;
        const float wy_start = cam_y - rx_start * sin_yaw + sy_rel * cos_yaw;

        float wx = wx_start;
        float wy = wy_start;
        const size_t row_idx = static_cast<size_t>(y) * m_width;

        for (uint32_t x = 0; x < m_width; ++x, wx += dWx, wy += dWy) {
            if (wx >= 0.0f && wx < map_w && wy >= 0.0f && wy < map_h) {
                uint32_t color = gnd ? gnd->sample_ground_color(wx, wy) : 0x00A39A8B;

                const int32_t cx = static_cast<int32_t>(wx);
                const int32_t cy = static_cast<int32_t>(wy);
                const auto* cell = gat.get_cell(cx, cy);

                if (cell) {
                    if (cell->type == world::CellType::NonWalkable) {
                        const uint32_t r = (((color >> 16) & 0xFF) * 55) / 100;
                        const uint32_t g = (((color >> 8)  & 0xFF) * 55) / 100;
                        const uint32_t b = ((color & 0xFF)         * 55) / 100;
                        color = (r << 16) | (g << 8) | b;
                    } else if (cell->type == world::CellType::WalkableWater || cell->type == world::CellType::WalkableWater2) {
                        const float wave = std::sin(wx * 0.7f + time_sec * 3.5f) * 16.0f;
                        const uint32_t b = std::clamp(static_cast<int32_t>(175 + wave), 0, 255);
                        const uint32_t g = std::clamp(static_cast<int32_t>(120 + wave * 0.6f), 0, 255);
                        color = (0x20 << 16) | (g << 8) | b;
                    } else {
                        const float slope = (cell->top_left - cell->bottom_right) * 3.2f;
                        const int32_t r = std::clamp(static_cast<int32_t>((color >> 16) & 0xFF) + static_cast<int32_t>(slope), 0, 255);
                        const int32_t g = std::clamp(static_cast<int32_t>((color >> 8)  & 0xFF) + static_cast<int32_t>(slope), 0, 255);
                        const int32_t b = std::clamp(static_cast<int32_t>(color & 0xFF)         + static_cast<int32_t>(slope), 0, 255);
                        color = (r << 16) | (g << 8) | b;
                    }
                }
                m_pixels[row_idx + x] = color;
                m_depth_buffer[row_idx + x] = sy_rel; // Continuous depth of ground along view axis
            } else {
                m_pixels[row_idx + x] = 0x00141419;
                m_depth_buffer[row_idx + x] = 1e9f;
            }
        }
    }
}

void SoftwareRenderer::draw_textured_triangle(
    float x0, float y0, float z0, float u0, float v0,
    float x1, float y1, float z1, float u1, float v1,
    float x2, float y2, float z2, float u2, float v2,
    const resource::BmpImage* texture
) {
    // Sort vertices by Y ascending (y0 <= y1 <= y2)
    if (y1 < y0) { std::swap(x0, x1); std::swap(y0, y1); std::swap(z0, z1); std::swap(u0, u1); std::swap(v0, v1); }
    if (y2 < y0) { std::swap(x0, x2); std::swap(y0, y2); std::swap(z0, z2); std::swap(u0, u2); std::swap(v0, v2); }
    if (y2 < y1) { std::swap(x1, x2); std::swap(y1, y2); std::swap(z1, z2); std::swap(u1, u2); std::swap(v1, v2); }

    const float total_height = y2 - y0;
    if (total_height < 0.5f) return;

    if (y2 < 0.0f || y0 >= static_cast<float>(m_height)) return;
    const float min_x_vert = std::min({x0, x1, x2});
    const float max_x_vert = std::max({x0, x1, x2});
    if (max_x_vert < 0.0f || min_x_vert >= static_cast<float>(m_width)) return;

    const int32_t start_y = std::max<int32_t>(0, static_cast<int32_t>(std::ceil(y0)));
    const int32_t end_y = std::min<int32_t>(static_cast<int32_t>(m_height - 1), static_cast<int32_t>(std::floor(y2)));

    for (int32_t y = start_y; y <= end_y; ++y) {
        const float fy = static_cast<float>(y);
        const float alpha = (fy - y0) / total_height;

        float xa = x0 + (x2 - x0) * alpha;
        float za = z0 + (z2 - z0) * alpha;
        float ua = u0 + (u2 - u0) * alpha;
        float va = v0 + (v2 - v0) * alpha;

        float xb, zb, ub, vb;
        if (fy < y1) {
            const float segment_h = y1 - y0;
            const float beta = (segment_h > 0.001f) ? ((fy - y0) / segment_h) : 1.0f;
            xb = x0 + (x1 - x0) * beta;
            zb = z0 + (z1 - z0) * beta;
            ub = u0 + (u1 - u0) * beta;
            vb = v0 + (v1 - v0) * beta;
        } else {
            const float segment_h = y2 - y1;
            const float beta = (segment_h > 0.001f) ? ((fy - y1) / segment_h) : 1.0f;
            xb = x1 + (x2 - x1) * beta;
            zb = z1 + (z2 - z1) * beta;
            ub = u1 + (u2 - u1) * beta;
            vb = v1 + (v2 - v1) * beta;
        }

        if (xa > xb) {
            std::swap(xa, xb);
            std::swap(za, zb);
            std::swap(ua, ub);
            std::swap(va, vb);
        }

        const float span_width = xb - xa;
        if (span_width < 0.5f) continue;

        const int32_t min_x = std::max<int32_t>(0, static_cast<int32_t>(std::ceil(xa)));
        const int32_t max_x = std::min<int32_t>(static_cast<int32_t>(m_width - 1), static_cast<int32_t>(std::floor(xb)));
        const size_t row_idx = static_cast<size_t>(y) * m_width;

        for (int32_t x = min_x; x <= max_x; ++x) {
            const float t = (static_cast<float>(x) - xa) / span_width;
            const float z = za + (zb - za) * t;

            if (z < m_depth_buffer[row_idx + x]) {
                const float u = ua + (ub - ua) * t;
                const float v = va + (vb - va) * t;

                uint32_t color = texture ? texture->sample(u, v) : 0x00D0C8B8;
                // RO transparency check: magenta key (0xFF00FF)
                if ((color & 0x00FFFFFF) == 0x00FF00FF) {
                    continue;
                }

                m_pixels[row_idx + x] = color;
                m_depth_buffer[row_idx + x] = z;
            }
        }
    }
}

void SoftwareRenderer::draw_rsm_model(
    const world::RsmModel& model,
    float rsw_x,
    float rsw_y,
    float rsw_z,
    float scale_x,
    float scale_y,
    float scale_z,
    float rot_x,
    float rot_y,
    float rot_z,
    float cam_x,
    float cam_y,
    float zoom,
    float yaw_deg,
    float pitch_deg
) {
    const float base_cell_size = 28.0f;
    const float cell_size = base_cell_size * zoom;
    if (cell_size <= 0.001f) return;

    const float center_sx = m_width * 0.5f;
    const float center_sy = m_height * 0.5f;

    const float rad_cam_yaw = -yaw_deg * (std::numbers::pi_v<float> / 180.0f);
    const float cos_cam_yaw = std::cos(rad_cam_yaw);
    const float sin_cam_yaw = std::sin(rad_cam_yaw);

    const float rad_cam_pitch = std::clamp(pitch_deg, 20.0f, 85.0f) * (std::numbers::pi_v<float> / 180.0f);
    const float sin_pitch = std::sin(rad_cam_pitch);
    const float cos_pitch = std::cos(rad_cam_pitch);

    // 3D rotation angles in radians: Yaw (rot_y), Pitch (rot_x), Roll (rot_z)
    // In official Ragexe (sub_ABB970): RotateY -> RotateX -> RotateZ
    const float rad_rot_y = rot_y * (std::numbers::pi_v<float> / 180.0f);
    const float cos_y = std::cos(rad_rot_y);
    const float sin_y = std::sin(rad_rot_y);

    const float rad_rot_x = rot_x * (std::numbers::pi_v<float> / 180.0f);
    const float cos_x = std::cos(rad_rot_x);
    const float sin_x = std::sin(rad_rot_x);

    const float rad_rot_z = rot_z * (std::numbers::pi_v<float> / 180.0f);
    const float cos_z = std::cos(rad_rot_z);
    const float sin_z = std::sin(rad_rot_z);

    const float min_visible_cy = (center_sy - static_cast<float>(m_height)) / (cell_size * sin_pitch) - 35.0f;

    const auto apply_node_transform = [](world::Vec3 point, const world::RsmNode& node) {
        point.x *= node.scale.x;
        point.y *= node.scale.y;
        point.z *= node.scale.z;

        const float axis_len = std::sqrt(
            node.rot_axis.x * node.rot_axis.x +
            node.rot_axis.y * node.rot_axis.y +
            node.rot_axis.z * node.rot_axis.z
        );
        if (axis_len > 0.0001f && std::abs(node.rot_angle) > 0.0001f) {
            const float ax = node.rot_axis.x / axis_len;
            const float ay = node.rot_axis.y / axis_len;
            const float az = node.rot_axis.z / axis_len;
            const float c = std::cos(node.rot_angle);
            const float s = std::sin(node.rot_angle);
            const float dot = ax * point.x + ay * point.y + az * point.z;
            const world::Vec3 source = point;
            point.x = source.x * c + (ay * source.z - az * source.y) * s + ax * dot * (1.0f - c);
            point.y = source.y * c + (az * source.x - ax * source.z) * s + ay * dot * (1.0f - c);
            point.z = source.z * c + (ax * source.y - ay * source.x) * s + az * dot * (1.0f - c);
        }

        point.x += node.offset_pos.x;
        point.y += node.offset_pos.y;
        point.z += node.offset_pos.z;
        return point;
    };

    for (const auto& node : model.nodes()) {
        const auto& verts = node.vertices;
        const auto& uvs = node.texcoords;
        const auto& faces = node.faces;
        if (verts.empty() || faces.empty()) continue;

        for (const auto& face : faces) {
            if (face.vertex_indices[0] >= verts.size() ||
                face.vertex_indices[1] >= verts.size() ||
                face.vertex_indices[2] >= verts.size()) {
                continue;
            }

            // Determine texture pointer
            const resource::BmpImage* tex_ptr = nullptr;
            if (face.texture_id < node.texture_indices.size()) {
                const int32_t g_idx = node.texture_indices[face.texture_id];
                if (g_idx >= 0 && static_cast<size_t>(g_idx) < model.textures().size()) {
                    tex_ptr = model.textures()[g_idx].get();
                }
            } else if (face.texture_id < model.textures().size()) {
                tex_ptr = model.textures()[face.texture_id].get();
            }

            float proj_x[3]{}, proj_y[3]{}, proj_z[3]{}, proj_u[3]{}, proj_v[3]{};

            for (int k = 0; k < 3; ++k) {
                world::Vec3 v = verts[face.vertex_indices[k]];
                const world::RsmNode* transform_node = &node;
                for (size_t depth = 0; transform_node && depth < model.nodes().size(); ++depth) {
                    v = apply_node_transform(v, *transform_node);
                    if (transform_node->parent_name.empty()) break;
                    const auto parent = std::find_if(
                        model.nodes().begin(), model.nodes().end(),
                        [&](const world::RsmNode& candidate) { return candidate.name == transform_node->parent_name; }
                    );
                    transform_node = parent == model.nodes().end() ? nullptr : &*parent;
                }

                // 1. Apply the RSW object's scale.
                const float sx = v.x * scale_x;
                const float sy = v.y * scale_y;
                const float sz = v.z * scale_z;

                // 2. Rotate (RotateY -> RotateX -> RotateZ as in Ragexe sub_ABB970)
                // RotateY (sub_553270):
                const float x1 = sx * cos_y + sz * sin_y;
                const float y1 = sy;
                const float z1 = -sx * sin_y + sz * cos_y;

                // RotateX (sub_552F70):
                const float x2 = x1;
                const float y2 = y1 * cos_x - z1 * sin_x;
                const float z2 = y1 * sin_x + z1 * cos_x;

                // RotateZ (sub_553570):
                const float rx = x2 * cos_z - y2 * sin_z;
                const float ry = x2 * sin_z + y2 * cos_z;
                const float rz = z2;

                // 3. Place at world position (RSW coordinates)
                const float px = rsw_x + rx;
                const float py = rsw_y + ry;
                const float pz = rsw_z + rz;

                // 4. Convert to GAT cell coordinates (center of Prontera 156x196 = 780, 980)
                const float wx = (px + 780.0f) / 5.0f;
                const float wy = (pz + 980.0f) / 5.0f;
                const float elev = -py; // Negative Y in RSM represents elevation above ground

                // 5. Camera relative delta
                const float dx = wx - cam_x;
                const float dy = wy - cam_y;

                // 6. Camera yaw rotation
                const float cx = dx * cos_cam_yaw - dy * sin_cam_yaw;
                const float cy = dx * sin_cam_yaw + dy * cos_cam_yaw;

                // 7. Screen projection
                proj_x[k] = center_sx + cx * cell_size;
                proj_y[k] = center_sy - cy * (cell_size * sin_pitch) - (elev / 5.0f) * (cell_size * cos_pitch);
                proj_z[k] = cy; // Continuous view distance along camera ground plane

                if (face.texcoord_indices[k] < uvs.size()) {
                    proj_u[k] = uvs[face.texcoord_indices[k]].u;
                    proj_v[k] = uvs[face.texcoord_indices[k]].v;
                }
            }

            // Viewport & near-plane rejection
            if ((proj_x[0] < -80.0f && proj_x[1] < -80.0f && proj_x[2] < -80.0f) ||
                (proj_x[0] > m_width + 80.0f && proj_x[1] > m_width + 80.0f && proj_x[2] > m_width + 80.0f) ||
                (proj_y[0] < -200.0f && proj_y[1] < -200.0f && proj_y[2] < -200.0f) ||
                (proj_y[0] > m_height + 80.0f && proj_y[1] > m_height + 80.0f && proj_y[2] > m_height + 80.0f)) {
                continue;
            }

            // Discard triangles entirely behind the visible camera frustum
            if (proj_z[0] < min_visible_cy && proj_z[1] < min_visible_cy && proj_z[2] < min_visible_cy) {
                continue;
            }

            // 2D backface culling for one-sided polygons (accounting for negative axis mirroring)
            const float cross_2d = (proj_x[1] - proj_x[0]) * (proj_y[2] - proj_y[0]) -
                                  (proj_y[1] - proj_y[0]) * (proj_x[2] - proj_x[0]);
            const float oriented_cross = ((scale_x * scale_y * scale_z) < 0.0f) ? -cross_2d : cross_2d;
            if (!face.two_sided && oriented_cross > 0.0f) {
                // Facing away
                continue;
            }

            draw_textured_triangle(
                proj_x[0], proj_y[0], proj_z[0], proj_u[0], proj_v[0],
                proj_x[1], proj_y[1], proj_z[1], proj_u[1], proj_v[1],
                proj_x[2], proj_y[2], proj_z[2], proj_u[2], proj_v[2],
                tex_ptr
            );
        }
    }
}

void SoftwareRenderer::draw_map_scenery(
    const world::RswMap& rsw,
    const std::unordered_map<std::string, std::shared_ptr<world::RsmModel>>& model_cache,
    float cam_x,
    float cam_y,
    float zoom,
    float yaw_deg,
    float pitch_deg
) {
    const auto& models = rsw.models();
    if (models.empty()) return;

    // View range frustum limit (~40 cells around camera)
    const float max_range_cells = 42.0f / (zoom > 0.5f ? std::sqrt(zoom) : 0.7f);

    for (const auto& obj : models) {
        const float cell_x = (obj.position.x + 780.0f) / 5.0f;
        const float cell_y = (obj.position.z + 980.0f) / 5.0f;

        if (std::abs(cell_x - cam_x) > max_range_cells ||
            std::abs(cell_y - cam_y) > max_range_cells) {
            continue;
        }

        auto it = model_cache.find(obj.filename);
        if (it == model_cache.end() || !it->second) {
            continue;
        }

        draw_rsm_model(
            *it->second,
            obj.position.x,
            obj.position.y,
            obj.position.z,
            obj.scale.x,
            obj.scale.y,
            obj.scale.z,
            obj.rotation.x,
            obj.rotation.y,
            obj.rotation.z,
            cam_x,
            cam_y,
            zoom,
            yaw_deg,
            pitch_deg
        );
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

bool SoftwareRenderer::save_to_bmp(const std::filesystem::path& path) const {
    HANDLE hFile = CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    BITMAPFILEHEADER bfh{};
    bfh.bfType = 0x4D42; // "BM"
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize = bfh.bfOffBits + static_cast<DWORD>(m_width * m_height * 4);

    BITMAPINFOHEADER bih{};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = static_cast<LONG>(m_width);
    bih.biHeight = -static_cast<LONG>(m_height); // Top-down
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    DWORD written = 0;
    WriteFile(hFile, &bfh, sizeof(bfh), &written, nullptr);
    WriteFile(hFile, &bih, sizeof(bih), &written, nullptr);
    WriteFile(hFile, m_pixels.data(), static_cast<DWORD>(m_pixels.size() * 4), &written, nullptr);
    CloseHandle(hFile);
    return true;
}

void SoftwareRenderer::draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t argb_color) {
    const uint32_t a = (argb_color >> 24) & 0xFF;
    if (a == 0) return;
    const uint32_t r_src = (argb_color >> 16) & 0xFF;
    const uint32_t g_src = (argb_color >> 8) & 0xFF;
    const uint32_t b_src = argb_color & 0xFF;

    const int32_t x0 = std::clamp(x, 0, static_cast<int32_t>(m_width));
    const int32_t y0 = std::clamp(y, 0, static_cast<int32_t>(m_height));
    const int32_t x1 = std::clamp(x + w, 0, static_cast<int32_t>(m_width));
    const int32_t y1 = std::clamp(y + h, 0, static_cast<int32_t>(m_height));

    if (a == 255) {
        const uint32_t c = (r_src << 16) | (g_src << 8) | b_src;
        for (int32_t py = y0; py < y1; ++py) {
            uint32_t* row = &m_pixels[py * m_width];
            for (int32_t px = x0; px < x1; ++px) {
                row[px] = c;
            }
        }
    } else {
        const uint32_t inv_a = 255 - a;
        for (int32_t py = y0; py < y1; ++py) {
            uint32_t* row = &m_pixels[py * m_width];
            for (int32_t px = x0; px < x1; ++px) {
                const uint32_t dst = row[px];
                const uint32_t r = (r_src * a + ((dst >> 16) & 0xFF) * inv_a) / 255;
                const uint32_t g = (g_src * a + ((dst >> 8) & 0xFF) * inv_a) / 255;
                const uint32_t b = (b_src * a + (dst & 0xFF) * inv_a) / 255;
                row[px] = (r << 16) | (g << 8) | b;
            }
        }
    }
}

} // namespace ro::render
