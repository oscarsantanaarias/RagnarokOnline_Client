#include "ro/resource/resource_mgr.hpp"
#include <windows.h>
#include <fstream>
#include <algorithm>

namespace ro::resource {

static std::filesystem::path to_native_path(std::string_view ansi_path) {
    if (ansi_path.empty()) return {};
    std::wstring wide;
    wide.reserve(ansi_path.size());
    for (char c : ansi_path) {
        wide.push_back(static_cast<wchar_t>(static_cast<uint8_t>(c)));
    }
    return std::filesystem::path(wide);
}

ResourceManager& ResourceManager::instance() {
    static ResourceManager s_instance;
    return s_instance;
}

bool ResourceManager::mount_grf(const std::filesystem::path& grf_path) {
    std::error_code ec;
    if (!std::filesystem::exists(grf_path, ec)) {
        return false;
    }

    auto grf = std::make_unique<GrfArchive>();
    if (!grf->open(grf_path)) {
        return false;
    }

    m_grfs.push_back(std::move(grf));
    return true;
}

void ResourceManager::set_data_folder(const std::filesystem::path& folder_path) {
    m_data_folder = folder_path;
}

bool ResourceManager::exists(const std::string& virtual_path) const {
    try {
        // 1. Normalize slashes
        std::string norm_path = virtual_path;
        std::replace(norm_path.begin(), norm_path.end(), '/', '\\');

        std::string stripped_path = norm_path;
        if (stripped_path.starts_with("data\\")) {
            stripped_path = stripped_path.substr(5);
        }

        const auto native_stripped = to_native_path(stripped_path);
        const auto native_norm = to_native_path(norm_path);

        std::error_code ec;
        if (std::filesystem::exists(m_data_folder / native_stripped, ec)) return true;
        if (std::filesystem::exists(m_data_folder / native_norm, ec)) return true;
        if (std::filesystem::exists(native_norm, ec)) return true;

        // 2. Check mounted GRFs
        for (const auto& grf : m_grfs) {
            if (!grf) continue;
            if (grf->contains(norm_path) || grf->contains("data\\" + stripped_path) || grf->contains(stripped_path)) {
                return true;
            }
        }
    } catch (...) {
        return false;
    }

    return false;
}

std::vector<uint8_t> ResourceManager::read_file(const std::string& virtual_path) {
    try {
        // 1. Normalize slashes
        std::string norm_path = virtual_path;
        std::replace(norm_path.begin(), norm_path.end(), '/', '\\');

        std::string stripped_path = norm_path;
        if (stripped_path.starts_with("data\\")) {
            stripped_path = stripped_path.substr(5);
        }

        const auto native_stripped = to_native_path(stripped_path);
        const auto native_norm = to_native_path(norm_path);

        // Check disk candidate paths in order
        std::vector<std::filesystem::path> disk_candidates = {
            m_data_folder / native_stripped,
            m_data_folder / native_norm,
            native_norm
        };

        std::error_code ec;
        for (const auto& disk_path : disk_candidates) {
            if (std::filesystem::exists(disk_path, ec) && !std::filesystem::is_directory(disk_path, ec)) {
                std::ifstream file(disk_path, std::ios::binary | std::ios::ate);
                if (file.is_open()) {
                    const auto size = file.tellg();
                    file.seekg(0, std::ios::beg);
                    std::vector<uint8_t> buffer(static_cast<size_t>(size));
                    file.read(reinterpret_cast<char*>(buffer.data()), size);
                    return buffer;
                }
            }
        }

        // 2. Fallback: Read directly from mounted GRF archives
        const std::vector<std::string> grf_keys = {
            norm_path,
            "data\\" + stripped_path,
            stripped_path
        };

        for (auto& grf : m_grfs) {
            if (!grf) continue;
            for (const auto& key : grf_keys) {
                if (grf->contains(key)) {
                    auto buf = grf->read_file(key);
                    if (!buf.empty()) {
                        return buf;
                    }
                }
            }
        }
    } catch (...) {
        return {};
    }

    return {};
}

} // namespace ro::resource

