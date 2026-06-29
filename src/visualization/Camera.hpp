#pragma once
//===-- visualization/Camera.hpp --------------------------------*- C++ -*-===//
// Orbit (3rd-person) camera. Always looks at `target`; the user can only
// rotate around that point with a mouse drag (yaw/pitch).
//===----------------------------------------------------------------------===//

#include <glm/glm.hpp>

namespace warehouse {

class OrbitCamera {
public:
    OrbitCamera() = default;

    void setTarget(const glm::vec3& t)        { m_target = t; }
    void setDistance(float d)                 { m_distance = d; }
    void setAngles(float yawRad, float pitchRad);

    /// Apply a delta from a mouse drag. `dxPx`/`dyPx` are in window pixels.
    void rotate(float dxPx, float dyPx);

    glm::vec3 getPosition() const;
    glm::mat4 getViewMatrix() const;

    const glm::vec3& target() const   { return m_target; }
    float            distance() const { return m_distance; }

private:
    glm::vec3 m_target{0.0f};
    float m_distance{10.0f};
    float m_yaw  {0.6f};   // radians
    float m_pitch{0.55f};  // radians, clamped to avoid flipping
    float m_sensitivity{0.005f};
};

} // namespace warehouse
