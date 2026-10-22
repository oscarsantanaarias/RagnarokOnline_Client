#include "ro/resource/grf.hpp"
#include "ro/resource/inflate.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace ro::resource {

#pragma pack(push, 1)
struct GrfHeaderRaw {
    char signature[16];
    uint8_t allow_encryption[14];
    uint32_t file_table_offset;
    uint32_t seed;
    uint32_t file_count;
    uint32_t version;
};
#pragma pack(pop)

std::string GrfArchive::normalize_path(std::string_view path) const {
    std::string s(path);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        if (c == '/') return '\\';
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool GrfArchive::open(const std::filesystem::path& grf_path) {
    close();

    std::ifstream file(grf_path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    if (!parse_header_and_index(file)) {
        return false;
    }

    m_path = grf_path;
    m_is_open = true;
    return true;
}

void GrfArchive::close() {
    m_entries.clear();
    m_is_open = false;
    m_path.clear();
}

bool GrfArchive::contains(const std::string& virtual_path) const {
    return m_entries.contains(normalize_path(virtual_path));
}

std::optional<GrfFileEntry> GrfArchive::get_entry(const std::string& virtual_path) const {
    const auto it = m_entries.find(normalize_path(virtual_path));
    if (it != m_entries.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool GrfArchive::parse_header_and_index(std::ifstream& file) {
    GrfHeaderRaw hdr{};
    file.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (file.gcount() != sizeof(hdr)) {
        return false;
    }

    // Support both classic "Master of Magic" and modern "Event Horizon" signatures
    if (std::strncmp(hdr.signature, "Master of Magic", 15) != 0 &&
        std::strncmp(hdr.signature, "Event Horizon", 13) != 0) {
        return false;
    }

    if (hdr.version != 0x0200 && hdr.version != 0x0100 && hdr.version != 0x0300) {
        return false;
    }

    file.seekg(0, std::ios::end);
    const uint64_t file_len = static_cast<uint64_t>(file.tellg());

    uint64_t table_pos = sizeof(GrfHeaderRaw) + static_cast<uint64_t>(hdr.file_table_offset);
    if (file_len > 0x100000000ULL && table_pos < 0x100000000ULL) {
        table_pos += 0x100000000ULL;
    }

    file.seekg(static_cast<std::streamoff>(table_pos), std::ios::beg);

    uint32_t comp_size = 0;
    uint32_t real_size = 0;
    file.read(reinterpret_cast<char*>(&comp_size), sizeof(comp_size));
    file.read(reinterpret_cast<char*>(&real_size), sizeof(real_size));

    if (comp_size == 0) {
        // Version 0x300 has a 4-byte padding field preceding comp_size
        file.seekg(static_cast<std::streamoff>(table_pos + 4), std::ios::beg);
        file.read(reinterpret_cast<char*>(&comp_size), sizeof(comp_size));
        file.read(reinterpret_cast<char*>(&real_size), sizeof(real_size));
    }

    if (comp_size == 0 || real_size == 0 || comp_size > 50000000) {
        return false;
    }

    std::vector<uint8_t> comp_buffer(comp_size);
    file.read(reinterpret_cast<char*>(comp_buffer.data()), comp_size);

    std::vector<uint8_t> decomp_buffer(real_size);
    if (!Inflater::decompress_zlib(comp_buffer, decomp_buffer)) {
        return false;
    }

    // Parse individual file entries from decompressed table buffer
    size_t cursor = 0;
    const uint32_t expected_files = hdr.file_count - hdr.seed - 7;

    for (uint32_t i = 0; i < expected_files && cursor < decomp_buffer.size(); ++i) {
        const char* name_ptr = reinterpret_cast<const char*>(&decomp_buffer[cursor]);
        const size_t name_len = std::strlen(name_ptr);
        cursor += name_len + 1;

        if (cursor + 17 > decomp_buffer.size()) break;

        GrfFileEntry entry{};
        entry.filename = name_ptr;
        std::memcpy(&entry.compressed_size, &decomp_buffer[cursor], 4);
        std::memcpy(&entry.compressed_size_aligned, &decomp_buffer[cursor + 4], 4);
        std::memcpy(&entry.real_size, &decomp_buffer[cursor + 8], 4);
        entry.flags = decomp_buffer[cursor + 12];
        std::memcpy(&entry.offset, &decomp_buffer[cursor + 13], 4);
        cursor += 17;

        // Skip folders (flags & 0x02)
        if (entry.flags & 0x01) {
            m_entries[normalize_path(entry.filename)] = std::move(entry);
        }
    }

    return true;
}

std::vector<uint8_t> GrfArchive::read_file(const std::string& virtual_path) {
    const auto entry_opt = get_entry(virtual_path);
    if (!entry_opt) {
        return {};
    }

    const auto& entry = *entry_opt;
    std::ifstream file(m_path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    const uint64_t file_pos = sizeof(GrfHeaderRaw) + entry.offset;
    file.seekg(static_cast<std::streamoff>(file_pos), std::ios::beg);

    std::vector<uint8_t> comp_data(entry.compressed_size_aligned);
    file.read(reinterpret_cast<char*>(comp_data.data()), entry.compressed_size_aligned);

    if (entry.compressed_size == entry.real_size) {
        comp_data.resize(entry.real_size);
        return comp_data;
    }

    std::vector<uint8_t> real_data(entry.real_size);
    if (Inflater::decompress_zlib(comp_data, real_data)) {
        return real_data;
    }

    return comp_data;
}

} // namespace ro::resource
