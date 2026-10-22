#pragma once

#include "ro/core/types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <span>
#include <optional>
#include <filesystem>

namespace ro::world {

enum class RswObjectType : int32_t {
    Model  = 1,
    Light  = 2,
    Sound  = 3,
    Effect = 4
};

#ifndef RO_WORLD_VEC3_DEFINED
#define RO_WORLD_VEC3_DEFINED
struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};
#endif

struct RswModelObject {
    std::string name;
    int32_t anim_type{0};
    float anim_speed{1.0f};
    int32_t block_type{0};
    std::string filename; // Path to .rsm 3D model
    std::string nodename;
    Vec3 position{};
    Vec3 rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct RswLightObject {
    std::string name;
    Vec3 position{};
    core::Color32 color{255, 255, 255, 255};
    float range{100.0f};
};

struct RswSoundObject {
    std::string name;
    std::string filename; // Path to .wav
    Vec3 position{};
    float volume{1.0f};
    int32_t width{0};
    int32_t height{0};
    float range{100.0f};
    float cycle{0.0f};
};

struct RswEffectObject {
    std::string name;
    Vec3 position{};
    int32_t effect_id{0};
    float emit_speed{1.0f};
    float param[4]{0.0f, 0.0f, 0.0f, 0.0f};
};

class RswMap {
public:
    RswMap() = default;

    [[nodiscard]] static std::optional<RswMap> load_from_memory(std::span<const uint8_t> buffer);
    [[nodiscard]] static std::optional<RswMap> load_from_file(const std::filesystem::path& file_path);

    [[nodiscard]] uint16_t version() const noexcept { return m_version; }
    [[nodiscard]] const std::string& gnd_filename() const noexcept { return m_gnd_file; }
    [[nodiscard]] const std::string& gat_filename() const noexcept { return m_gat_file; }
    [[nodiscard]] const std::string& bgm_filename() const noexcept { return m_bgm_file; }

    [[nodiscard]] float water_level() const noexcept { return m_water_level; }
    [[nodiscard]] const std::vector<RswModelObject>& models() const noexcept { return m_models; }
    [[nodiscard]] const std::vector<RswLightObject>& lights() const noexcept { return m_lights; }
    [[nodiscard]] const std::vector<RswSoundObject>& sounds() const noexcept { return m_sounds; }
    [[nodiscard]] const std::vector<RswEffectObject>& effects() const noexcept { return m_effects; }

private:
    uint16_t m_version{0x0201};
    std::string m_ini_file;
    std::string m_gnd_file;
    std::string m_gat_file;
    std::string m_bgm_file;

    float m_water_level{0.0f};
    int32_t m_water_type{0};
    float m_wave_height{0.0f};
    float m_wave_speed{0.0f};
    float m_wave_pitch{0.0f};
    float m_water_anim_speed{0.0f};

    int32_t m_light_longitude{45};
    int32_t m_light_latitude{45};
    Vec3 m_diffuse_color{1.0f, 1.0f, 1.0f};
    Vec3 m_ambient_color{0.3f, 0.3f, 0.3f};

    std::vector<RswModelObject> m_models;
    std::vector<RswLightObject> m_lights;
    std::vector<RswSoundObject> m_sounds;
    std::vector<RswEffectObject> m_effects;
};

} // namespace ro::world
