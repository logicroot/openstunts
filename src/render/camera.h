#pragma once
// Fly camera for debugging and free-look gameplay views.

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace s3d {

class Camera {
 public:
  glm::vec3 position{0.0f};
  float yaw_deg = -90.0f;   // rotation around Y, -90 looks down -Z
  float pitch_deg = 0.0f;   // rotation around X
  float fov_deg = 65.0f;
  float znear = 8.0f;
  float zfar = 200000.0f;

  glm::vec3 forward() const {
    const float y = glm::radians(yaw_deg);
    const float p = glm::radians(pitch_deg);
    return glm::normalize(glm::vec3(std::cos(y) * std::cos(p), std::sin(p),
                                    std::sin(y) * std::cos(p)));
  }

  glm::vec3 right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
  }

  glm::vec3 up() const { return glm::normalize(glm::cross(right(), forward())); }

  glm::mat4 view() const {
    return glm::lookAt(position, position + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
  }

  glm::mat4 projection(float aspect) const {
    return glm::perspective(glm::radians(fov_deg), aspect, znear, zfar);
  }

  void move(const glm::vec3& delta) { position += delta; }

  void add_mouse(float dx, float dy, float sensitivity = 0.12f) {
    yaw_deg += dx * sensitivity;
    pitch_deg -= dy * sensitivity;
    if (pitch_deg > 89.0f) pitch_deg = 89.0f;
    if (pitch_deg < -89.0f) pitch_deg = -89.0f;
  }
};

}  // namespace s3d
