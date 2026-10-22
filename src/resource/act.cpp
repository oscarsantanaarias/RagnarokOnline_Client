#include "ro/resource/act.hpp"
#include <cstring>
#include <algorithm>

namespace ro::resource {

std::optional<ActResource> ActResource::load_from_memory(std::span<const uint8_t> buffer) {
    // Ragnarok .act header is 16 bytes (sub_7437B0 reads 0x10)
    if (buffer.size() < 16) {
        return std::nullopt;
    }

    if (buffer[0] != 'A' || buffer[1] != 'C') {
        return std::nullopt;
    }

    ActResource res{};
    std::memcpy(&res.m_version, &buffer[2], 2);

    uint16_t action_count = 0;
    std::memcpy(&action_count, &buffer[4], 2);
    if (action_count > 1024) {
        return std::nullopt;
    }

    size_t cursor = 16; // Actions start at offset 16 (0x10)
    res.m_actions.resize(action_count);

    // Read Actions
    for (uint16_t a = 0; a < action_count; ++a) {
        if (cursor + 4 > buffer.size()) break;
        uint32_t motion_count = 0;
        std::memcpy(&motion_count, &buffer[cursor], 4);
        cursor += 4;

        if (motion_count > 1024) break;

        auto& action = res.m_actions[a];
        action.motions.resize(motion_count);

        for (uint32_t m = 0; m < motion_count; ++m) {
            auto& motion = action.motions[m];

            // 1. Range bounds (32 bytes = 2 x 0x10)
            if (cursor + 32 > buffer.size()) break;
            cursor += 32;

            // 2. Clips count
            if (cursor + 4 > buffer.size()) break;
            uint32_t clip_count = 0;
            std::memcpy(&clip_count, &buffer[cursor], 4);
            cursor += 4;

            if (clip_count > 512) break;

            motion.clips.resize(clip_count);
            for (uint32_t c = 0; c < clip_count; ++c) {
                auto& clip = motion.clips[c];

                if (cursor + 16 > buffer.size()) break;
                std::memcpy(&clip.x, &buffer[cursor], 4);
                std::memcpy(&clip.y, &buffer[cursor + 4], 4);
                std::memcpy(&clip.spr_index, &buffer[cursor + 8], 4);
                std::memcpy(&clip.mirror, &buffer[cursor + 12], 4);
                cursor += 16;

                if (res.m_version >= 0x0200) {
                    if (cursor + 4 > buffer.size()) break;
                    std::memcpy(&clip.color, &buffer[cursor], 4);
                    cursor += 4;
                }

                if (res.m_version >= 0x0204) {
                    if (cursor + 8 > buffer.size()) break;
                    std::memcpy(&clip.scale_x, &buffer[cursor], 4);
                    std::memcpy(&clip.scale_y, &buffer[cursor + 4], 4);
                    cursor += 8;
                } else if (res.m_version >= 0x0200) {
                    if (cursor + 4 > buffer.size()) break;
                    std::memcpy(&clip.scale_x, &buffer[cursor], 4);
                    clip.scale_y = clip.scale_x;
                    cursor += 4;
                }

                if (res.m_version >= 0x0200) {
                    if (cursor + 4 > buffer.size()) break;
                    std::memcpy(&clip.rotation, &buffer[cursor], 4);
                    cursor += 4;
                }

                if (res.m_version >= 0x0200) {
                    if (cursor + 4 > buffer.size()) break;
                    std::memcpy(&clip.spr_type, &buffer[cursor], 4);
                    cursor += 4;
                }

                if (res.m_version >= 0x0205) {
                    if (cursor + 8 > buffer.size()) break;
                    std::memcpy(&clip.width, &buffer[cursor], 4);
                    std::memcpy(&clip.height, &buffer[cursor + 4], 4);
                    cursor += 8;
                }
            }

            // 3. Sound trigger event (v2.0+)
            if (res.m_version >= 0x0200) {
                if (cursor + 4 <= buffer.size()) {
                    std::memcpy(&motion.sound_trigger, &buffer[cursor], 4);
                    cursor += 4;
                }
            }

            // 4. Attach points (v2.3+ reads 0x10 = 16 bytes per attach point)
            if (res.m_version >= 0x0203) {
                if (cursor + 4 <= buffer.size()) {
                    uint32_t attach_count = 0;
                    std::memcpy(&attach_count, &buffer[cursor], 4);
                    cursor += 4;

                    if (attach_count <= 64) {
                        motion.attach_points.resize(attach_count);
                        for (uint32_t at = 0; at < attach_count; ++at) {
                            if (cursor + 16 > buffer.size()) break;
                            // 16 bytes: unknown(4), x(4), y(4), attr(4)
                            std::memcpy(&motion.attach_points[at].x, &buffer[cursor + 4], 4);
                            std::memcpy(&motion.attach_points[at].y, &buffer[cursor + 8], 4);
                            std::memcpy(&motion.attach_points[at].attr, &buffer[cursor + 12], 4);
                            cursor += 16;
                        }
                    }
                }
            }
        }
    }

    // Sound files table (v2.1+)
    if (res.m_version >= 0x0201 && cursor + 4 <= buffer.size()) {
        uint32_t sound_count = 0;
        std::memcpy(&sound_count, &buffer[cursor], 4);
        cursor += 4;

        if (sound_count <= 256) {
            res.m_sound_files.resize(sound_count);
            for (uint32_t s = 0; s < sound_count; ++s) {
                if (cursor + 40 > buffer.size()) break;
                const char* name = reinterpret_cast<const char*>(&buffer[cursor]);
                res.m_sound_files[s] = std::string(name, ::strnlen(name, 40));
                cursor += 40;
            }
        }
    }

    // Delay factors per action (v2.2+)
    if (res.m_version >= 0x0202 && cursor + (action_count * 4) <= buffer.size()) {
        res.m_delays.resize(action_count);
        for (uint16_t a = 0; a < action_count; ++a) {
            std::memcpy(&res.m_delays[a], &buffer[cursor], 4);
            cursor += 4;
        }
    }

    return res;
}

} // namespace ro::resource
