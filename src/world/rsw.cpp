#include "ro/world/rsw.hpp"
#include <cstring>

namespace ro::world {

std::optional<RswMap> RswMap::load_from_memory(std::span<const uint8_t> buffer) {
    if (buffer.size() < 40) {
        return std::nullopt;
    }

    if (buffer[0] != 'G' || buffer[1] != 'R' || buffer[2] != 'S' || buffer[3] != 'W') {
        return std::nullopt;
    }

    RswMap map{};
    map.m_version = static_cast<uint16_t>(buffer[4] | (buffer[5] << 8));

    // File name entries are 40 bytes each in Ragnarok RSW
    const char* gnd_ptr = reinterpret_cast<const char*>(&buffer[6]);
    map.m_gnd_file = std::string(gnd_ptr, ::strnlen(gnd_ptr, 40));

    const char* gat_ptr = reinterpret_cast<const char*>(&buffer[46]);
    map.m_gat_file = std::string(gat_ptr, ::strnlen(gat_ptr, 40));

    const char* src_ptr = reinterpret_cast<const char*>(&buffer[86]);
    map.m_bgm_file = std::string(src_ptr, ::strnlen(src_ptr, 40));

    size_t cursor = 126;

    // Water parameters (v1.3+)
    if (map.m_version >= 0x0103 && cursor + 20 <= buffer.size()) {
        std::memcpy(&map.m_water_level, &buffer[cursor], 4);
        std::memcpy(&map.m_water_type, &buffer[cursor + 4], 4);
        std::memcpy(&map.m_wave_height, &buffer[cursor + 8], 4);
        std::memcpy(&map.m_wave_speed, &buffer[cursor + 12], 4);
        std::memcpy(&map.m_wave_pitch, &buffer[cursor + 16], 4);
        cursor += 20;
    }

    // Light parameters (v1.5+)
    if (map.m_version >= 0x0105 && cursor + 24 <= buffer.size()) {
        cursor += 24; // skip light ambient/diffuse angles
    }

    // Ground bounds (v1.6+)
    if (map.m_version >= 0x0106 && cursor + 16 <= buffer.size()) {
        cursor += 16;
    }

    // Objects placed in map
    if (cursor + 4 > buffer.size()) {
        return map;
    }

    uint32_t object_count = 0;
    std::memcpy(&object_count, &buffer[cursor], 4);
    cursor += 4;

    for (uint32_t i = 0; i < object_count; ++i) {
        if (cursor + 4 > buffer.size()) break;

        int32_t obj_type = 0;
        std::memcpy(&obj_type, &buffer[cursor], 4);
        cursor += 4;

        if (obj_type == static_cast<int32_t>(RswObjectType::Model)) {
            // Type 1: 3D Model (.rsm)
            if (cursor + 168 > buffer.size()) break;

            RswModelObject model{};
            const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
            model.name = std::string(name_p, ::strnlen(name_p, 40));
            cursor += 80;

            const char* file_p = reinterpret_cast<const char*>(&buffer[cursor]);
            model.filename = std::string(file_p, ::strnlen(file_p, 40));
            cursor += 40;

            cursor += 40; // skip node names

            std::memcpy(&model.position, &buffer[cursor], 12);
            std::memcpy(&model.rotation, &buffer[cursor + 12], 12);
            std::memcpy(&model.scale, &buffer[cursor + 24], 12);
            cursor += 36;

            map.m_models.push_back(std::move(model));
        } else if (obj_type == static_cast<int32_t>(RswObjectType::Light)) {
            // Type 2: Light source
            if (cursor + 68 > buffer.size()) break;
            RswLightObject light{};
            const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
            light.name = std::string(name_p, ::strnlen(name_p, 40));
            cursor += 40;

            std::memcpy(&light.position, &buffer[cursor], 12);
            std::memcpy(&light.color, &buffer[cursor + 12], 4);
            std::memcpy(&light.range, &buffer[cursor + 16], 4);
            cursor += 20;

            map.m_lights.push_back(std::move(light));
        } else if (obj_type == static_cast<int32_t>(RswObjectType::Sound)) {
            // Type 3: Sound source (.wav)
            if (cursor + 124 > buffer.size()) break;
            RswSoundObject snd{};
            const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
            snd.name = std::string(name_p, ::strnlen(name_p, 40));
            cursor += 40;

            const char* file_p = reinterpret_cast<const char*>(&buffer[cursor]);
            snd.filename = std::string(file_p, ::strnlen(file_p, 40));
            cursor += 40;

            std::memcpy(&snd.position, &buffer[cursor], 12);
            std::memcpy(&snd.volume, &buffer[cursor + 12], 4);
            std::memcpy(&snd.width, &buffer[cursor + 16], 4);
            std::memcpy(&snd.height, &buffer[cursor + 20], 4);
            std::memcpy(&snd.range, &buffer[cursor + 24], 4);
            std::memcpy(&snd.cycle, &buffer[cursor + 28], 4);
            cursor += 32;

            map.m_sounds.push_back(std::move(snd));
        } else if (obj_type == static_cast<int32_t>(RswObjectType::Effect)) {
            // Type 4: Effect
            if (cursor + 60 > buffer.size()) break;
            RswEffectObject eff{};
            const char* name_p = reinterpret_cast<const char*>(&buffer[cursor]);
            eff.name = std::string(name_p, ::strnlen(name_p, 40));
            cursor += 40;

            std::memcpy(&eff.position, &buffer[cursor], 12);
            std::memcpy(&eff.effect_id, &buffer[cursor + 12], 4);
            std::memcpy(&eff.emit_speed, &buffer[cursor + 16], 4);
            cursor += 20;

            map.m_effects.push_back(std::move(eff));
        }
    }

    return map;
}

} // namespace ro::world

