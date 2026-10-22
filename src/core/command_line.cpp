#include "ro/core/command_line.hpp"
#include <regex>
#include <cstdint>

namespace ro::core {

CommandLineOptions CommandLineParser::parse(std::string_view cmd_line) {
    CommandLineOptions opt{};
    const std::string s(cmd_line);

    if (s.find("/stove_live") != std::string::npos) {
        opt.stove_live = true;
    }
    if (s.find("/stove_sandbox") != std::string::npos) {
        opt.stove_sandbox = true;
    }
    if (s.find("-1K") != std::string::npos) {
        opt.windowed = true;
    }

    // Parse -t:<token>
    std::regex token_re(R"(-t:([^\s]+))");
    std::smatch token_match;
    if (std::regex_search(s, token_match, token_re) && token_match.size() > 1) {
        opt.token = token_match[1].str();
    }

    // Parse -inf:<id>
    std::regex inf_re(R"(-inf:(\d+))");
    std::smatch inf_match;
    if (std::regex_search(s, inf_match, inf_re) && inf_match.size() > 1) {
        opt.server_id = static_cast<uint32_t>(std::stoul(inf_match[1].str()));
    }

    return opt;
}

} // namespace ro::core
