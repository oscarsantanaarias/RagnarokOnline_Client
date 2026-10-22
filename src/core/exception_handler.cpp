#include "ro/core/exception_handler.hpp"
#include <windows.h>
#include <dbghelp.h>
#include <chrono>
#include <format>
#include <filesystem>

#pragma comment(lib, "dbghelp.lib")

namespace ro::core {

namespace {
    LPTOP_LEVEL_EXCEPTION_FILTER g_prev_filter{nullptr};
    std::string g_prefix{"RagnarokCrash"};

    LONG WINAPI top_level_filter(EXCEPTION_POINTERS* ep) {
        const auto now = std::chrono::system_clock::now();
        const auto time_str = std::format("{0:%Y%m%d_%H%M%S}", now);
        const std::string filename = std::format("{}_{}.dmp", g_prefix, time_str);

        HANDLE file = CreateFileA(filename.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei{};
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = ep;
            mei.ClientPointers = TRUE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                              MiniDumpWithDataSegs, &mei, nullptr, nullptr);
            CloseHandle(file);
        }

        return EXCEPTION_EXECUTE_HANDLER;
    }
}

bool ExceptionHandler::install(const std::string& dump_prefix) {
    g_prefix = dump_prefix;
    g_prev_filter = SetUnhandledExceptionFilter(top_level_filter);
    return true;
}

void ExceptionHandler::uninstall() {
    if (g_prev_filter) {
        SetUnhandledExceptionFilter(g_prev_filter);
        g_prev_filter = nullptr;
    }
}

} // namespace ro::core

