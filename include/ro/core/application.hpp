#pragma once

#include "ro/core/types.hpp"
#include "ro/core/command_line.hpp"
#include "ro/resource/grf.hpp"
#include <windows.h>
#include <memory>

namespace ro::core {

class Application {
public:
    Application(HINSTANCE instance, std::string_view cmd_line, int show_cmd);
    ~Application();

    // Initializes Windows window, Mutex, ExceptionHandler, and GRF filesystem
    bool initialize();

    // Runs the main game loop (PeekMessage + CModeMgr::process)
    int run();

    [[nodiscard]] HWND window_handle() const noexcept { return m_hwnd; }
    [[nodiscard]] resource::GrfArchive& grf() noexcept { return m_grf; }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    HINSTANCE m_instance{nullptr};
    std::string m_cmd_line;
    int m_show_cmd{SW_SHOW};
    HWND m_hwnd{nullptr};
    HANDLE m_mutex{nullptr};

    CommandLineOptions m_options;
    resource::GrfArchive m_grf;
    UINT m_timer_resolution{0};
};

} // namespace ro::core

