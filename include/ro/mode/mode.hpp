#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <windows.h>

namespace ro::mode {

enum class ModeType : uint32_t {
    Login = 0,
    Game  = 1,
    None  = 0xFFFFFFFF
};

class CMode {
public:
    virtual ~CMode() = default;

    virtual bool on_init(std::string_view map_name) = 0;
    virtual bool on_exit() = 0;
    virtual bool on_run() = 0;
    virtual bool on_update(float delta_time) = 0;
    virtual void on_draw() = 0;
    virtual intptr_t on_msg(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) = 0;
    virtual void on_change_state(int32_t new_state) = 0;

    [[nodiscard]] virtual ModeType type() const noexcept = 0;
};

} // namespace ro::mode

