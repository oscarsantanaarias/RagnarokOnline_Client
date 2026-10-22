#pragma once

#include "ro/core/types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>

namespace ro::resource {

struct GrfFileEntry {
    std::string filename;
    uint32_t compressed_size{0};
    uint32_t compressed_size_aligned{0};
    uint32_t real_size{0};
    uint8_t flags{0};
    uint32_t offset{0};
};

class GrfArchive {
public:
    GrfArchive() = default;
    ~GrfArchive() = default;

    // Open GRF file from disk and load its index table into memory
    bool open(const std::filesystem::path& grf_path);
    void close();

    [[nodiscard]] bool is_open() const noexcept { return m_is_open; }
    [[nodiscard]] size_t file_count() const noexcept { return m_entries.size(); }

    [[nodiscard]] bool contains(const std::string& virtual_path) const;
    [[nodiscard]] std::optional<GrfFileEntry> get_entry(const std::string& virtual_path) const;

    // Extract an asset directly into an in-memory byte buffer
    [[nodiscard]] std::vector<uint8_t> read_file(const std::string& virtual_path);

private:
    std::string normalize_path(std::string_view path) const;
    bool parse_header_and_index(std::ifstream& file);

    std::filesystem::path m_path;
    std::unordered_map<std::string, GrfFileEntry> m_entries;
    bool m_is_open{false};
};

} // namespace ro::resource

