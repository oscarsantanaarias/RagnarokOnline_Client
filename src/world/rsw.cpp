#include "ro/world/rsw.hpp"
#include <windows.h>
#include <cstring>
#include <algorithm>

namespace ro::world {

std::optional<RswMap> RswMap::load_from_file(const std::filesystem::path& file_path) {
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
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart == 0 || fileSize.QuadPart > 100 * 1024 * 1024) {
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

    return load_from_memory(buffer);
}

std::optional<RswMap> RswMap::load_from_memory(std::span<const uint8_t> buffer) {
    if (buffer.size() < 40) {
        return std::nullopt;
    }

    if (buffer[0] != 'G' || buffer[1] != 'R' || buffer[2] != 'S' || buffer[3] != 'W') {
        return std::nullopt;
    }

    RswMap map{};
    const uint8_t v_maj = buffer[4];
    const uint8_t v_min = buffer[5];
    map.m_version = static_cast<uint16_t>(v_maj | (v_min << 8));

    size_t cursor = 6;

    // Optional build number (v2.5+)
    uint32_t build_number = 0;
    if (v_maj == 2 && v_min >= 5) {
        if (cursor + 4 > buffer.size()) return std::nullopt;
        std::memcpy(&build_number, &buffer[cursor], 4);
        cursor += 4;
    }

    // Optional unknown flag (v2.2+)
    if (v_maj == 2 && v_min >= 2) {
        if (cursor + 1 > buffer.size()) return std::nullopt;
        cursor += 1;
    }

    // File names (40 bytes each)
    if (cursor + 80 > buffer.size()) return std::nullopt;
    const char* ini_ptr = reinterpret_cast<const char*>(&buffer[cursor]);
    map.m_ini_file = std::string(ini_ptr, ::strnlen(ini_ptr, 40));
    cursor += 40;

    const char* gnd_ptr = reinterpret_cast<const char*>(&buffer[cursor]);
    map.m_gnd_file = std::string(gnd_ptr, ::strnlen(gnd_ptr, 40));
    cursor += 40;

    if (v_maj > 1 || (v_maj == 1 && v_min >= 4)) {
        if (cursor + 80 > buffer.size()) return std::nullopt;
        const char* gat_ptr = reinterpret_cast<const char*>(&buffer[cursor]);
        map.m_gat_file = std::string(gat_ptr, ::strnlen(gat_ptr, 40));
        cursor += 40;

        const char* src_ptr = reinterpret_cast<const char*>(&buffer[cursor]);
        map.m_bgm_file = std::string(src_ptr, ::strnlen(src_ptr, 40));
        cursor += 40;
    }

    // Water settings (skipped if v2.6+)
    if (!(v_maj == 2 && v_min >= 6)) {
        if (v_maj > 1 || (v_maj == 1 && v_min >= 3)) {
            if (cursor + 4 <= buffer.size()) {
                std::memcpy(&map.m_water_level, &buffer[cursor], 4);
                cursor += 4;
            }
        }
        if (v_maj > 1 || (v_maj == 1 && v_min >= 8)) {
            if (cursor + 16 <= buffer.size()) {
                std::memcpy(&map.m_water_type, &buffer[cursor], 4);
                std::memcpy(&map.m_wave_height, &buffer[cursor + 4], 4);
                std::memcpy(&map.m_wave_speed, &buffer[cursor + 8], 4);
                std::memcpy(&map.m_wave_pitch, &buffer[cursor + 12], 4);
                cursor += 16;
            }
        }
        if (v_maj > 1 || (v_maj == 1 && v_min >= 9)) {
            if (cursor + 4 <= buffer.size()) {
                std::memcpy(&map.m_water_anim_speed, &buffer[cursor], 4);
                cursor += 4;
            }
        }
    }

    // Light parameters (v1.5+)
    if (v_maj > 1 || (v_maj == 1 && v_min >= 5)) {
        if (cursor + 32 <= buffer.size()) {
            std::memcpy(&map.m_light_longitude, &buffer[cursor], 4);
            std::memcpy(&map.m_light_latitude, &buffer[cursor + 4], 4);
            std::memcpy(&map.m_diffuse_color, &buffer[cursor + 8], 12);
            std::memcpy(&map.m_ambient_color, &buffer[cursor + 20], 12);
            cursor += 32;
        }
    }

    // Ground bounds (v1.7+: 4 bytes, v1.6+: 16 bytes)
    if (v_maj >= 2 || (v_maj == 1 && v_min >= 7)) {
        cursor += 4;
    }
    if (v_maj >= 2 || (v_maj == 1 && v_min >= 6)) {
        cursor += 16;
    }

    // Quadtree skip for v2.7+
    if (v_maj >= 2 && v_min >= 7 && build_number >= 0xDD) {
        if (cursor + 4 <= buffer.size()) {
            uint32_t quad_size = 0;
            std::memcpy(&quad_size, &buffer[cursor], 4);
            cursor += 4 + quad_size * 4;
        }
    }

    // Object count
    if (cursor + 4 > buffer.size()) {
        return map;
    }

    uint32_t object_count = 0;
    std::memcpy(&object_count, &buffer[cursor], 4);
    cursor += 4;

    map.m_models.reserve(std::min<size_t>(object_count, 4096));

    for (uint32_t i = 0; i < object_count; ++i) {
        if (cursor + 4 > buffer.size()) break;

        int32_t obj_type = 0;
        std::memcpy(&obj_type, &buffer[cursor], 4);
        cursor += 4;

        if (obj_type == static_cast<int32_t>(RswObjectType::Model)) {
            // Type 1: 3D Model (.rsm)
            if (cursor + 40 > buffer.size()) break;
            RswModelObject model{};
            const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
            model.name = std::string(name_p, ::strnlen(name_p, 40));
            cursor += 40;

            if (v_maj > 1 || (v_maj == 1 && v_min >= 3)) {
                if (cursor + 12 > buffer.size()) break;
                std::memcpy(&model.anim_type, &buffer[cursor], 4);
                std::memcpy(&model.anim_speed, &buffer[cursor + 4], 4);
                std::memcpy(&model.block_type, &buffer[cursor + 8], 4);
                cursor += 12;
            }

            if ((v_maj > 2 || (v_maj == 2 && v_min >= 6)) && build_number >= 0xBA) {
                cursor += 1;
            }
            if (v_maj >= 2 && v_min >= 7) {
                cursor += 4;
            }

            // Filename (80 bytes), NodeName (80 bytes), pos (12), rot (12), scale (12) = 196 bytes
            if (cursor + 196 > buffer.size()) break;
            const char* file_p = reinterpret_cast<const char*>(&buffer[cursor]);
            model.filename = std::string(file_p, ::strnlen(file_p, 80));
            cursor += 80;

            const char* node_p = reinterpret_cast<const char*>(&buffer[cursor]);
            model.nodename = std::string(node_p, ::strnlen(node_p, 80));
            cursor += 80;

            std::memcpy(&model.position, &buffer[cursor], 12);
            std::memcpy(&model.rotation, &buffer[cursor + 12], 12);
            std::memcpy(&model.scale, &buffer[cursor + 24], 12);
            cursor += 36;

            map.m_models.push_back(std::move(model));
        } else if (obj_type == static_cast<int32_t>(RswObjectType::Light)) {
            // Type 2: Light source (80 name + 12 pos + 12 color + 4 range = 108 bytes)
            if (cursor + 108 > buffer.size()) break;
            RswLightObject light{};
            const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
            light.name = std::string(name_p, ::strnlen(name_p, 80));
            cursor += 80;

            std::memcpy(&light.position, &buffer[cursor], 12);
            std::memcpy(&light.color, &buffer[cursor + 12], 12);
            std::memcpy(&light.range, &buffer[cursor + 24], 4);
            cursor += 28;

            map.m_lights.push_back(std::move(light));
        } else if (obj_type == static_cast<int32_t>(RswObjectType::Sound)) {
            // Type 3: Sound source (.wav) (80 name + 80 file + 12 pos + 20 params = 192 bytes)
            if (cursor + 192 > buffer.size()) break;
            RswSoundObject snd{};
            const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
            snd.name = std::string(name_p, ::strnlen(name_p, 80));
            cursor += 80;

            const char* file_p = reinterpret_cast<const char*>(&buffer[cursor]);
            snd.filename = std::string(file_p, ::strnlen(file_p, 80));
            cursor += 80;

            std::memcpy(&snd.position, &buffer[cursor], 12);
            std::memcpy(&snd.volume, &buffer[cursor + 12], 4);
            std::memcpy(&snd.width, &buffer[cursor + 16], 4);
            std::memcpy(&snd.height, &buffer[cursor + 20], 4);
            std::memcpy(&snd.range, &buffer[cursor + 24], 4);
            std::memcpy(&snd.cycle, &buffer[cursor + 28], 4);
            cursor += 32;

            map.m_sounds.push_back(std::move(snd));
        } else if (obj_type == static_cast<int32_t>(RswObjectType::Effect)) {
            // Type 4: Effect (80 name + 12 pos + 4 id + 4 speed + 16 params = 116 bytes)
            if (cursor + 116 > buffer.size()) break;
            RswEffectObject eff{};
            const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
            eff.name = std::string(name_p, ::strnlen(name_p, 80));
            cursor += 80;

            std::memcpy(&eff.position, &buffer[cursor], 12);
            std::memcpy(&eff.effect_id, &buffer[cursor + 12], 4);
            std::memcpy(&eff.emit_speed, &buffer[cursor + 16], 4);
            std::memcpy(eff.param, &buffer[cursor + 20], 16);
            cursor += 36;

            map.m_effects.push_back(std::move(eff));
        } else {
            // Unknown object type, can't continue reliably
            break;
        }
    }

    return map;
}

} // namespace ro::world
