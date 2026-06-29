//===-- visualization/Camera.cpp --------------------------------*- C++ -*-===//
#include "Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace warehouse {

namespace {
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kPitchLimit = 1.4f; // ~80 degrees
} // namespace

void OrbitCamera::setAngles(float yawRad, float pitchRad) {
    m_yaw   = yawRad;
    m_pitch = std::clamp(pitchRad, -kPitchLimit, kPitchLimit);
}

void OrbitCamera::rotate(float dxPx, float dyPx) {
    m_yaw   -= dxPx * m_sensitivity;
    m_pitch += dyPx * m_sensitivity;
    m_pitch = std::clamp(m_pitch, -kPitchLimit, kPitchLimit);
    // Wrap yaw to keep numbers bounded.
    while (m_yaw >  kPi) m_yaw -= 2.0f * kPi;
    while (m_yaw < -kPi) m_yaw += 2.0f * kPi;
}

glm::vec3 OrbitCamera::getPosition() const {
    const float cp = std::cos(m_pitch);
    const float sp = std::sin(m_pitch);
    const float cy = std::cos(m_yaw);
    const float sy = std::sin(m_yaw);
    glm::vec3 dir(cp * sy, sp, cp * cy);
    return m_target + dir * m_distance;
}

glm::mat4 OrbitCamera::getViewMatrix() const {
    return glm::lookAt(getPosition(), m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

} // namespace warehouse
