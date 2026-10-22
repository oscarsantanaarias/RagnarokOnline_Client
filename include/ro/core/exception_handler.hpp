#pragma once

#include <string>

namespace ro::core {

class ExceptionHandler {
public:
    static bool install(const std::string& dump_prefix = "RagnarokCrash");
    static void uninstall();
};

} // namespace ro::core

