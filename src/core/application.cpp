#include "ro/core/application.hpp"
#include "ro/core/exception_handler.hpp"
#include "ro/mode/mode_mgr.hpp"
#include "ro/resource/resource_mgr.hpp"
#include <objbase.h>
#include <timeapi.h>
#include <iostream>
#include <fstream>

namespace ro::core {

namespace {
    constexpr const char* kWindowClassName = "Ragnarok";
    constexpr const char* kWindowTitle = "Ragnarok Online";
}

Application::Application(HINSTANCE instance, std::string_view cmd_line, int show_cmd)
    : m_instance(instance), m_cmd_line(cmd_line), m_show_cmd(show_cmd) {
    m_options = CommandLineParser::parse(cmd_line);
}

Application::~Application() {
    if (m_timer_resolution != 0) {
        timeEndPeriod(m_timer_resolution);
    }
    if (m_mutex) {
        ReleaseMutex(m_mutex);
        CloseHandle(m_mutex);
    }
    CoUninitialize();
}

bool Application::initialize() {
    std::ofstream dbg("debug_init.log");
    dbg << "Step 1: Mutex..." << std::endl;
    // 1. Single Instance Check via Named Mutex
    m_mutex = CreateMutexA(nullptr, TRUE, m_options.mutex_name.c_str());
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        dbg << "Warning: Mutex already exists (ignoring for offline test)" << std::endl;
    }

    dbg << "Step 2: Timer..." << std::endl;
    // 2. High-precision Timer
    TIMECAPS tc{};
    if (timeGetDevCaps(&tc, sizeof(tc)) == TIMERR_NOERROR) {
        m_timer_resolution = std::min(std::max(tc.wPeriodMin, 1u), tc.wPeriodMax);
        timeBeginPeriod(m_timer_resolution);
    }

    dbg << "Step 3: COM..." << std::endl;
    // 3. COM Subsystem
    CoInitialize(nullptr);

    dbg << "Step 4: Exception Handler..." << std::endl;
    // 4. Exception Handler / Crash Dumper
    ExceptionHandler::install();

    dbg << "Step 5: Resource Pipeline (Raw disk files mode)..." << std::endl;
    // 5. In-Memory Resource Pipeline (Pure raw folder mode: data/)
    auto& res_mgr = resource::ResourceManager::instance();
    res_mgr.set_data_folder("data");
    dbg << "Resource folder set to: data" << std::endl;

    dbg << "Step 6: Register Class..." << std::endl;
    // 6. Register Window Class
    WNDCLASSA wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = m_instance;
    wc.hIcon = LoadIconA(m_instance, MAKEINTRESOURCEA(123)); // Ragnarok app icon
    wc.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassA(&wc)) {
        dbg << "Failed at RegisterClassA. Error: " << GetLastError() << std::endl;
        return false;
    }

    dbg << "Step 7: Window rect..." << std::endl;
    // 7. Calculate Window Rect & Screen Centering
    constexpr int default_width = 1024;
    constexpr int default_height = 768;

    RECT rc{ 0, 0, default_width, default_height };
    const DWORD style = m_options.windowed
        ? (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE)
        : (WS_POPUP | WS_VISIBLE);

    AdjustWindowRect(&rc, style, FALSE);

    const int screen_w = GetSystemMetrics(SM_CXSCREEN);
    const int screen_h = GetSystemMetrics(SM_CYSCREEN);
    const int win_w = rc.right - rc.left;
    const int win_h = rc.bottom - rc.top;
    const int pos_x = (screen_w - win_w) / 2;
    const int pos_y = (screen_h - win_h) / 2;

    dbg << "Step 8: CreateWindowExA..." << std::endl;
    // 8. Create Window
    m_hwnd = CreateWindowExA(
        0,
        kWindowClassName,
        kWindowTitle,
        style,
        pos_x, pos_y,
        win_w, win_h,
        nullptr,
        nullptr,
        m_instance,
        this
    );

    if (!m_hwnd) {
        dbg << "Failed at CreateWindowExA. Error: " << GetLastError() << std::endl;
        return false;
    }

    ShowWindow(m_hwnd, m_show_cmd);
    UpdateWindow(m_hwnd);

    dbg << "Initialization Success!" << std::endl;
    dbg.flush();
    return true;
}

int Application::run() {
    return mode::CModeMgr::instance().run(mode::ModeType::Login, "login.rsw");
}

LRESULT CALLBACK Application::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTA*>(lparam);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }

    // Forward message to active CMode
    if (mode::CModeMgr::instance().dispatch_msg(hwnd, msg, wparam, lparam) != 0) {
        return 0;
    }

    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SETCURSOR:
            SetCursor(LoadCursorA(nullptr, IDC_ARROW));
            return TRUE;

        default:
            return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}

} // namespace ro::core

