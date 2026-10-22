#include "ro/camera/camera.hpp"
#include <cmath>
#include <numbers>

namespace ro::camera {

world::Vec3 OrbitCamera::eye_position() const noexcept {
    const float rad_orbit = m_orbit_angle * (std::numbers::pi_v<float> / 180.0f);
    const float rad_tilt  = m_tilt_angle  * (std::numbers::pi_v<float> / 180.0f);

    const float cos_tilt = std::cos(rad_tilt);
    const float sin_tilt = std::sin(rad_tilt);

    return world::Vec3{
        .x = m_target.x + m_distance * cos_tilt * std::sin(rad_orbit),
        .y = m_target.y + m_distance * cos_tilt * std::cos(rad_orbit),
        .z = m_target.z + m_distance * sin_tilt
    };
}

} // namespace ro::camera

