#pragma once

#include "ro/core/types.hpp"
#include "ro/resource/spr.hpp"
#include "ro/resource/act.hpp"
#include "ro/resource/bmp.hpp"
#include "ro/actor/actor.hpp"
#include "ro/world/gat.hpp"
#include "ro/world/gnd.hpp"
#include "ro/world/rsw.hpp"
#include "ro/world/rsm.hpp"
#include <windows.h>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <memory>

namespace ro::render {

class SoftwareRenderer {
public:
    SoftwareRenderer(uint32_t width = 1024, uint32_t height = 768);
    ~SoftwareRenderer() = default;

    void resize(uint32_t width, uint32_t height);
    void clear(uint32_t color_xrgb = 0x001A1A24);

    // Blits a single sprite frame onto the framebuffer with alpha blending, clipping and scaling
    void draw_sprite(const resource::SprResource& spr, size_t frame_index, int32_t x, int32_t y,
                     bool mirror = false, core::Color32 tint = {255, 255, 255, 255}, float scale = 1.0f);

    // Renders an entire composite Actor (Body action clips + Head/Hair anchored clips) with zoom scale and camera yaw
    void draw_composite_actor(const resource::SprResource* body_spr, const resource::ActResource* body_act,
                             const resource::SprResource* head_spr, const resource::ActResource* head_act,
                             actor::ActionType action, actor::Direction dir, size_t motion_idx,
                             int32_t center_x, int32_t center_y, float scale = 1.0f, float camera_yaw_deg = 0.0f);

    // Renders the Ragnarok map ground with continuous affine texture mapping, yaw rotation, and pitch perspective
    void draw_map_ground(
        const world::GatCollision& gat,
        const world::GndMap* gnd,
        float cam_x,
        float cam_y,
        float zoom,
        float yaw_deg = 0.0f,
        float pitch_deg = 45.0f,
        float time_sec = 0.0f
    );

    // Rasterizes an affine-mapped textured 3D triangle with Z-buffer depth test and magenta/alpha transparency
    void draw_textured_triangle(
        float x0, float y0, float z0, float u0, float v0,
        float x1, float y1, float z1, float u1, float v1,
        float x2, float y2, float z2, float u2, float v2,
        const resource::BmpImage* texture
    );

    // Renders an RSM 3D mesh model placed at world coordinates with rotation, scaling, and camera projection
    void draw_rsm_model(
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
    );

    // Renders all world scenery objects from the RSW file within camera range
    void draw_map_scenery(
        const world::RswMap& rsw,
        const std::unordered_map<std::string, std::shared_ptr<world::RsmModel>>& model_cache,
        float cam_x,
        float cam_y,
        float zoom,
        float yaw_deg,
        float pitch_deg
    );

    // Draws a solid or alpha-blended rectangle (sub_54E460)
    void draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t argb_color);

    // Presents framebuffer to window HDC
    void present(HWND hwnd);

    // Saves current framebuffer to a 32-bit BMP image on disk
    bool save_to_bmp(const std::filesystem::path& path) const;

    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }

private:
    uint32_t m_width{1024};
    uint32_t m_height{768};
    std::vector<uint32_t> m_pixels; // XRGB32
    std::vector<float> m_depth_buffer; // Z-buffer (depth along camera view direction)
    BITMAPINFO m_bmi{};
};

} // namespace ro::render
