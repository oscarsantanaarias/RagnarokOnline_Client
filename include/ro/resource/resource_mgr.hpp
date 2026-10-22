#pragma once

#include "ro/resource/grf.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <optional>

namespace ro::resource {

class ResourceManager {
public:
    static ResourceManager& instance();

    // Mounts a .grf archive file into the in-memory lookup system
    bool mount_grf(const std::filesystem::path& grf_path);

    // Sets local folder root for loose files (e.g. "data" or "./")
    void set_data_folder(const std::filesystem::path& folder_path);

    // Reads file bytes with "Folder First, GRF Fallback" strategy:
    // 1. If exists on disk as loose file, reads directly from disk.
    // 2. Otherwise, decompresses/reads directly from mounted in-memory GRF.
    [[nodiscard]] std::vector<uint8_t> read_file(const std::string& virtual_path);

    [[nodiscard]] bool exists(const std::string& virtual_path) const;

    [[nodiscard]] const std::vector<std::unique_ptr<GrfArchive>>& grfs() const noexcept { return m_grfs; }

private:
    ResourceManager() = default;
    ~ResourceManager() = default;

    std::filesystem::path m_data_folder{"data"};
    std::vector<std::unique_ptr<GrfArchive>> m_grfs;
};

} // namespace ro::resource

