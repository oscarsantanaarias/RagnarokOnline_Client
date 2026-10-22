#pragma once

#include "ro/core/types.hpp"
#include "ro/resource/spr.hpp"
#include "ro/resource/act.hpp"
#include "ro/actor/actor.hpp"
#include <windows.h>
#include <vector>
#include <cstdint>

namespace ro::render {

class SoftwareRenderer {
public:
    SoftwareRenderer(uint32_t width = 1024, uint32_t height = 768);
    ~SoftwareRenderer() = default;

    void resize(uint32_t width, uint32_t height);
    void clear(uint32_t color_xrgb = 0x001A1A24);

    // Blits a single sprite frame onto the framebuffer with alpha blending and clipping
    void draw_sprite(const resource::SprResource& spr, size_t frame_index, int32_t x, int32_t y,
                     bool mirror = false, core::Color32 tint = {255, 255, 255, 255});

    // Renders an entire composite Actor (Body action clips + Head/Hair anchored clips)
    void draw_composite_actor(const resource::SprResource* body_spr, const resource::ActResource* body_act,
                             const resource::SprResource* head_spr, const resource::ActResource* head_act,
                             actor::ActionType action, actor::Direction dir, size_t motion_idx,
                             int32_t center_x, int32_t center_y);

    // Presents framebuffer to window HDC
    void present(HWND hwnd);

    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }

private:
    uint32_t m_width{1024};
    uint32_t m_height{768};
    std::vector<uint32_t> m_pixels; // XRGB32
    BITMAPINFO m_bmi{};
};

} // namespace ro::render
