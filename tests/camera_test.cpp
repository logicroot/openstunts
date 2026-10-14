#include "render/camera.h"
#include "tiny_test.h"

#include <cmath>

#include <glm/glm.hpp>

namespace {

// Project a world point and return its normalised device coordinates. +x is
// screen-right, +y is screen-up.
glm::vec3 ndc_of(const s3d::Camera& cam, const glm::vec3& p, float aspect) {
  const glm::vec4 clip = cam.projection(aspect) * cam.view() * glm::vec4(p, 1.0f);
  return glm::vec3(clip) / clip.w;
}

// A camera at the origin looking down +Z (the drive scene's forward axis).
s3d::Camera forward_camera() {
  s3d::Camera cam;
  cam.position = glm::vec3(0.0f);
  cam.yaw_deg = 90.0f;
  cam.pitch_deg = 0.0f;
  return cam;
}

}  // namespace

// The original engine projects with world +X on screen-right. GL lookAt is
// right-handed, so this guards the deliberate left-handed flip in view().
TEST(camera_world_plus_x_is_screen_right) {
  const s3d::Camera cam = forward_camera();
  const glm::vec3 ndc = ndc_of(cam, glm::vec3(10.0f, 0.0f, 100.0f), 16.0f / 9.0f);
  EXPECT_TRUE(ndc.x > 0.0f);
}

TEST(camera_world_minus_x_is_screen_left) {
  const s3d::Camera cam = forward_camera();
  const glm::vec3 ndc = ndc_of(cam, glm::vec3(-10.0f, 0.0f, 100.0f), 16.0f / 9.0f);
  EXPECT_TRUE(ndc.x < 0.0f);
}

TEST(camera_world_plus_y_is_screen_up) {
  const s3d::Camera cam = forward_camera();
  const glm::vec3 ndc = ndc_of(cam, glm::vec3(0.0f, 10.0f, 100.0f), 16.0f / 9.0f);
  EXPECT_TRUE(ndc.y > 0.0f);
}

// `right()` must agree with what the view actually puts on the right, so the
// debug fly camera strafes the way the screen reads.
TEST(camera_right_helper_matches_screen_right) {
  const s3d::Camera cam = forward_camera();
  const glm::vec3 point = cam.position + cam.right() * 10.0f + cam.forward() * 100.0f;
  const glm::vec3 ndc = ndc_of(cam, point, 16.0f / 9.0f);
  EXPECT_TRUE(ndc.x > 0.0f);
}

// Regression: a car turning right moves toward +X, and that must appear to the
// right of the camera that chases it down +Z.
TEST(camera_right_turn_moves_screen_right) {
  const s3d::Camera cam = forward_camera();
  const glm::vec3 car_ahead = glm::vec3(0.0f, 0.0f, 100.0f);
  const glm::vec3 car_after_right_turn = glm::vec3(20.0f, 0.0f, 100.0f);
  const float before = ndc_of(cam, car_ahead, 16.0f / 9.0f).x;
  const float after = ndc_of(cam, car_after_right_turn, 16.0f / 9.0f).x;
  EXPECT_TRUE(after > before);
}

// Moving the mouse right must swing the free-look view toward screen-right.
TEST(camera_mouse_right_turns_view_right) {
  s3d::Camera cam = forward_camera();
  const glm::vec3 before = cam.forward();
  const glm::vec3 screen_right = cam.right();
  cam.add_mouse(10.0f, 0.0f);
  const glm::vec3 turn = cam.forward() - before;
  EXPECT_TRUE(glm::dot(turn, screen_right) > 0.0f);
}

// Regression: the cockpit camera must face the car's nose, not backwards and
// not up at the sky. The car places its nose at R_y(-yaw)*+Z = (-sin, 0, cos)
// for the yaw given in radians (0x400 units per turn).
TEST(camera_cockpit_yaw_faces_car_nose) {
  constexpr float kAngleToRadians = 6.28318530718f / 1024.0f;
  for (int units : {0, 128, 256, 300, 512, 640, 768, 896, 1023}) {
    const float yaw = static_cast<float>(units) * kAngleToRadians;
    s3d::Camera cam;
    cam.pitch_deg = 0.0f;
    cam.yaw_deg = s3d::nose_forward_yaw_deg(yaw);
    const glm::vec3 nose(-std::sin(yaw), 0.0f, std::cos(yaw));
    // A flipped sign would score -1 here; a 90-degree error would score 0.
    EXPECT_TRUE(glm::dot(cam.forward(), nose) > 0.999f);
  }
}
