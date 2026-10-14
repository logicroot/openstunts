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
  // Horizontal field of view in degrees. The original engine builds its
  // projection from a horizontal half-angle (set_projection(0x28, 0x28, 320,
  // 200) in the racing view, i.e. an 80 degree horizontal FOV), so the port
  // stores the *horizontal* FOV and derives the vertical one from the window
  // aspect in projection(). This keeps the car the same size on screen
  // regardless of how wide the window is.
  float fov_deg = 80.0f;
  float znear = 8.0f;
  float zfar = 200000.0f;

  glm::vec3 forward() const {
    const float y = glm::radians(yaw_deg);
    const float p = glm::radians(pitch_deg);
    return glm::normalize(glm::vec3(std::cos(y) * std::cos(p), std::sin(p),
                                    std::sin(y) * std::cos(p)));
  }

  // The original engine renders through a left-handed camera: world +X maps to
  // screen-right, +Y up and +Z into the screen (see vector_to_point in the
  // restunts math.c port, where screen x = world_x * k / depth + centre). GL's
  // lookAt is right-handed, so `view()` mirrors its X axis to match; screen
  // right is therefore `up_hint x forward`, not `forward x up_hint`.
  glm::vec3 right() const {
    return glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward()));
  }

  glm::vec3 up() const { return glm::normalize(glm::cross(forward(), right())); }

  glm::mat4 view() const {
    glm::mat4 v =
        glm::lookAt(position, position + forward(), glm::vec3(0.0f, 1.0f, 0.0f));
    // Negate the camera-space X row to flip GL's right-handed basis into the
    // original engine's left-handed one. Face winding is unaffected because the
    // renderer does not cull back faces.
    for (int c = 0; c < 4; ++c) v[c][0] = -v[c][0];
    return v;
  }

  glm::mat4 projection(float aspect) const {
    // fov_deg is the horizontal FOV; GL's perspective() wants the vertical one.
    // tan(v/2) = tan(h/2) / aspect.
    const float a = (aspect > 0.0f) ? aspect : 1.0f;
    const float vfov = 2.0f * std::atan(std::tan(glm::radians(fov_deg) * 0.5f) / a);
    return glm::perspective(vfov, aspect, znear, zfar);
  }

  void move(const glm::vec3& delta) { position += delta; }

  void add_mouse(float dx, float dy, float sensitivity = 0.12f) {
    // Under the left-handed view, increasing yaw swings the nose toward -X,
    // i.e. screen-left, so mouse-right must decrease yaw to look right.
    yaw_deg -= dx * sensitivity;
    pitch_deg -= dy * sensitivity;
    if (pitch_deg > 89.0f) pitch_deg = 89.0f;
    if (pitch_deg < -89.0f) pitch_deg = -89.0f;
  }
};

// Yaw (in degrees) that makes a Camera's forward() point down a car's nose.
// The car model places its nose at R_y(-yaw)*+Z = (-sin yaw, 0, cos yaw) for a
// yaw in radians, whereas Camera::forward() points along +Z at yaw = +90 deg
// (see forward_camera in the camera tests). The cockpit camera therefore adds a
// quarter turn; the sign is not flipped, or the view faces backwards.
inline float nose_forward_yaw_deg(float nose_yaw_rad) {
  return glm::degrees(nose_yaw_rad) + 90.0f;
}

}  // namespace s3d
