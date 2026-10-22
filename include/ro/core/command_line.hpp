#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ro::core {

struct CommandLineOptions {
    std::string token;
    uint32_t server_id{0};
    bool stove_live{false};
    bool stove_sandbox{false};
    bool windowed{true};
    std::string grf_filename{"data.grf"};
    std::string skin_directory{"skin\\default"};
    std::string mutex_name{"MutexInit"};
};

class CommandLineParser {
public:
    static CommandLineOptions parse(std::string_view cmd_line);
};

} // namespace ro::core
