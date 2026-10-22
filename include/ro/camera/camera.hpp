#pragma once

#include "ro/world/rsw.hpp"
#include <algorithm>

namespace ro::camera {

class OrbitCamera {
public:
    OrbitCamera() = default;

    void set_target(float x, float y, float z) noexcept {
        m_target = {x, y, z};
    }

    void rotate(float delta_deg) noexcept {
        m_orbit_angle += delta_deg;
        while (m_orbit_angle >= 360.0f) m_orbit_angle -= 360.0f;
        while (m_orbit_angle < 0.0f)    m_orbit_angle += 360.0f;
    }

    void tilt(float delta_deg) noexcept {
        m_tilt_angle = std::clamp(m_tilt_angle + delta_deg, 15.0f, 75.0f);
    }

    void zoom(float delta_dist) noexcept {
        m_distance = std::clamp(m_distance + delta_dist, 50.0f, 800.0f);
    }

    [[nodiscard]] world::Vec3 eye_position() const noexcept;
    [[nodiscard]] world::Vec3 target() const noexcept { return m_target; }

    [[nodiscard]] float orbit_angle() const noexcept { return m_orbit_angle; }
    [[nodiscard]] float tilt_angle() const noexcept { return m_tilt_angle; }
    [[nodiscard]] float distance() const noexcept { return m_distance; }

private:
    world::Vec3 m_target{0.0f, 0.0f, 0.0f};
    float m_orbit_angle{0.0f};  // 0 to 360 deg
    float m_tilt_angle{45.0f};   // 15 to 75 deg
    float m_distance{250.0f};   // Distance from target
};

} // namespace ro::camera

