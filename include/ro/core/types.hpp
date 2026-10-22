#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <memory>
#include <array>

namespace ro::core {

struct Color24 {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
};

struct Color32 {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};
};

using Palette = std::array<Color32, 256>;

struct Point {
    int32_t x{0};
    int32_t y{0};
};

struct Rect {
    int32_t left{0};
    int32_t top{0};
    int32_t right{0};
    int32_t bottom{0};

    [[nodiscard]] constexpr int32_t width() const noexcept { return right - left; }
    [[nodiscard]] constexpr int32_t height() const noexcept { return bottom - top; }
};

struct Resolution {
    uint32_t width{1024};
    uint32_t height{768};
    uint32_t bpp{32};
    bool fullscreen{false};
};

} // namespace ro::core

