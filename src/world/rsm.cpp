#include "ro/world/rsm.hpp"
#include <windows.h>
#include <cstring>
#include <algorithm>

namespace ro::world {

static std::filesystem::path to_native_path(std::string_view ansi_path) {
    if (ansi_path.empty()) return {};
    std::wstring wide;
    wide.reserve(ansi_path.size());
    for (char c : ansi_path) {
        wide.push_back(static_cast<wchar_t>(static_cast<uint8_t>(c)));
    }
    return std::filesystem::path(wide);
}

std::optional<RsmModel> RsmModel::load_from_file(
    const std::filesystem::path& file_path,
    const std::filesystem::path& texture_base_dir
) {
    HANDLE hFile = CreateFileW(
        file_path.wstring().c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0 || fileSize.QuadPart > 50 * 1024 * 1024) {
        CloseHandle(hFile);
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(static_cast<size_t>(fileSize.QuadPart));
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) || bytesRead != buffer.size()) {
        CloseHandle(hFile);
        return std::nullopt;
    }
    CloseHandle(hFile);

    return load_from_memory(buffer, texture_base_dir);
}

std::optional<RsmModel> RsmModel::load_from_memory(
    std::span<const uint8_t> buffer,
    const std::filesystem::path& texture_base_dir
) {
    if (buffer.size() < 30) {
        return std::nullopt;
    }

    if (buffer[0] != 'G' || buffer[1] != 'R' || buffer[2] != 'S' || buffer[3] != 'M') {
        return std::nullopt;
    }

    RsmModel model{};
    const uint8_t v_maj = buffer[4];
    const uint8_t v_min = buffer[5];
    model.m_version = static_cast<uint16_t>(v_maj | (v_min << 8));

    size_t cursor = 6;
    if (cursor + 8 > buffer.size()) return std::nullopt;
    std::memcpy(&model.m_anim_len, &buffer[cursor], 4);
    std::memcpy(&model.m_shade_type, &buffer[cursor + 4], 4);
    cursor += 8;

    if (v_min >= 4) {
        if (cursor + 1 > buffer.size()) return std::nullopt;
        model.m_alpha = buffer[cursor];
        cursor += 1;
    }

    // Skip 16 reserved bytes
    cursor += 16;
    if (cursor + 4 > buffer.size()) return std::nullopt;

    uint32_t tex_count = 0;
    std::memcpy(&tex_count, &buffer[cursor], 4);
    cursor += 4;

    model.m_texture_names.resize(tex_count);
    model.m_textures.resize(tex_count);

    for (size_t i = 0; i < tex_count; ++i) {
        if (cursor + 40 > buffer.size()) return std::nullopt;
        const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
        const size_t len = ::strnlen(name_p, 40);
        std::string raw_name(name_p, len);
        model.m_texture_names[i] = raw_name;
        cursor += 40;

        // Attempt loading texture bitmap from disk
        const auto native_tex_path = to_native_path(raw_name);
        auto bmp_opt = resource::BmpImage::load_from_file(texture_base_dir / native_tex_path);
        if (!bmp_opt) {
            // Strip leading slashes if present
            std::string clean = raw_name;
            while (!clean.empty() && (clean.front() == '\\' || clean.front() == '/')) {
                clean.erase(0, 1);
            }
            bmp_opt = resource::BmpImage::load_from_file(texture_base_dir / to_native_path(clean));
        }
        if (!bmp_opt) {
            // Check direct file basename in texture_base_dir
            const auto base_name = native_tex_path.filename();
            bmp_opt = resource::BmpImage::load_from_file(texture_base_dir / base_name);
        }

        if (bmp_opt) {
            model.m_textures[i] = std::make_shared<resource::BmpImage>(std::move(*bmp_opt));
        }
    }

    if (cursor + 44 > buffer.size()) return std::nullopt;
    const char* main_p = reinterpret_cast<const char*>(&buffer[cursor]);
    model.m_main_node = std::string(main_p, ::strnlen(main_p, 40));
    cursor += 40;

    uint32_t node_count = 0;
    std::memcpy(&node_count, &buffer[cursor], 4);
    cursor += 4;

    model.m_nodes.resize(node_count);

    for (uint32_t ni = 0; ni < node_count; ++ni) {
        if (cursor + 80 > buffer.size()) return std::nullopt;

        auto& node = model.m_nodes[ni];
        const char* n_p = reinterpret_cast<const char*>(&buffer[cursor]);
        node.name = std::string(n_p, ::strnlen(n_p, 40));
        cursor += 40;

        const char* p_p = reinterpret_cast<const char*>(&buffer[cursor]);
        node.parent_name = std::string(p_p, ::strnlen(p_p, 40));
        cursor += 40;

        if (cursor + 4 > buffer.size()) return std::nullopt;
        uint32_t t_count = 0;
        std::memcpy(&t_count, &buffer[cursor], 4);
        cursor += 4;

        if (cursor + t_count * 4 > buffer.size()) return std::nullopt;
        node.texture_indices.resize(t_count);
        if (t_count > 0) {
            std::memcpy(node.texture_indices.data(), &buffer[cursor], t_count * 4);
            cursor += t_count * 4;
        }

        // 88 bytes transforms: matrix (48), offset (12), rot_angle (4), rot_axis (12), scale (12)
        if (cursor + 88 > buffer.size()) return std::nullopt;
        std::memcpy(node.transform_matrix, &buffer[cursor], 48);
        std::memcpy(&node.offset_pos, &buffer[cursor + 48], 12);
        std::memcpy(&node.rot_angle, &buffer[cursor + 60], 4);
        std::memcpy(&node.rot_axis, &buffer[cursor + 64], 12);
        std::memcpy(&node.scale, &buffer[cursor + 76], 12);
        cursor += 88;

        // Vertices
        if (cursor + 4 > buffer.size()) return std::nullopt;
        uint32_t v_count = 0;
        std::memcpy(&v_count, &buffer[cursor], 4);
        cursor += 4;

        if (cursor + v_count * 12 > buffer.size()) return std::nullopt;
        node.vertices.resize(v_count);
        if (v_count > 0) {
            std::memcpy(node.vertices.data(), &buffer[cursor], v_count * 12);
            cursor += v_count * 12;

            // Ragexe bakes the node's 3x4 bind matrix into every vertex while loading.
            const float* m = node.transform_matrix;
            for (auto& vertex : node.vertices) {
                const Vec3 source = vertex;
                vertex.x = source.x * m[0] + source.y * m[3] + source.z * m[6] + m[9];
                vertex.y = source.x * m[1] + source.y * m[4] + source.z * m[7] + m[10];
                vertex.z = source.x * m[2] + source.y * m[5] + source.z * m[8] + m[11];
            }
        }

        // Texcoords
        if (cursor + 4 > buffer.size()) return std::nullopt;
        uint32_t tc_count = 0;
        std::memcpy(&tc_count, &buffer[cursor], 4);
        cursor += 4;

        node.texcoords.resize(tc_count);
        for (uint32_t tci = 0; tci < tc_count; ++tci) {
            if (v_min == 1) {
                if (cursor + 8 > buffer.size()) return std::nullopt;
                std::memcpy(&node.texcoords[tci].u, &buffer[cursor], 4);
                std::memcpy(&node.texcoords[tci].v, &buffer[cursor + 4], 4);
                cursor += 8;
            } else {
                if (cursor + 12 > buffer.size()) return std::nullopt;
                std::memcpy(&node.texcoords[tci].u, &buffer[cursor + 4], 4);
                std::memcpy(&node.texcoords[tci].v, &buffer[cursor + 8], 4);
                cursor += 12;
            }
        }

        // Faces
        if (cursor + 4 > buffer.size()) return std::nullopt;
        uint32_t fc_count = 0;
        std::memcpy(&fc_count, &buffer[cursor], 4);
        cursor += 4;

        node.faces.resize(fc_count);
        for (uint32_t fci = 0; fci < fc_count; ++fci) {
            if (v_min == 1) {
                if (cursor + 20 > buffer.size()) return std::nullopt;
                std::memcpy(node.faces[fci].vertex_indices, &buffer[cursor], 6);
                std::memcpy(node.faces[fci].texcoord_indices, &buffer[cursor + 6], 6);
                std::memcpy(&node.faces[fci].texture_id, &buffer[cursor + 12], 2);
                std::memcpy(&node.faces[fci].padding, &buffer[cursor + 14], 2);
                std::memcpy(&node.faces[fci].smooth_group, &buffer[cursor + 16], 4);
                cursor += 20;
            } else {
                if (cursor + 24 > buffer.size()) return std::nullopt;
                std::memcpy(node.faces[fci].vertex_indices, &buffer[cursor], 6);
                std::memcpy(node.faces[fci].texcoord_indices, &buffer[cursor + 6], 6);
                std::memcpy(&node.faces[fci].texture_id, &buffer[cursor + 12], 2);
                std::memcpy(&node.faces[fci].padding, &buffer[cursor + 14], 2);
                std::memcpy(&node.faces[fci].two_sided, &buffer[cursor + 16], 4);
                std::memcpy(&node.faces[fci].smooth_group, &buffer[cursor + 20], 4);
                cursor += 24;
            }
        }

        // Rot keyframes
        if (cursor + 4 > buffer.size()) return std::nullopt;
        uint32_t rot_frame_count = 0;
        std::memcpy(&rot_frame_count, &buffer[cursor], 4);
        cursor += 4;
        cursor += rot_frame_count * 20;
    }

    return model;
}

} // namespace ro::world

