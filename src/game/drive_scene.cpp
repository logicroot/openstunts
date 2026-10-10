#include "game/drive_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "core/log.h"
#include "core/paths.h"
#include "physics/car_engine.h"
#include "physics/car_motion.h"
#include "physics/car_steering.h"
#include "physics/fixed_math.h"
#include "physics/wrap_math.h"
#include "platform/input.h"
#include "render/renderer.h"
#include "render/screenshot.h"
#include "world/car_mesh.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"
#include "world/track_elevation.h"
#include "world/track_mesh.h"
#include "world/track_shapes.h"
#include "world/world_units.h"

namespace s3d {

namespace {

constexpr double kPhysicsDt = 1.0 / 20.0;
constexpr int kMaxPhysicsStepsPerFrame = 4;
constexpr float kCarBodyDrop = 6.0f;  // pivot rests 6 above the wheel contact plane

// Follow-camera rates (restunts gamestep.c), per 20 Hz physics tick.
constexpr float kFollowDistance = 450.0f;
constexpr float kFollowHeight = 270.0f;
constexpr float kFollowAimHeight = 60.0f;
constexpr float kFollowHorizontalStep = 120.0f;
constexpr float kFollowVerticalStep = 30.0f;

constexpr float kAngleToRadians = 6.28318530718f / 1024.0f;

constexpr glm::vec4 kPlayerCarColor(0.85f, 0.60f, 0.10f, 1.0f);

void aim_at(Camera& cam, const glm::vec3& target) {
  const glm::vec3 d = glm::normalize(target - cam.position);
  cam.yaw_deg = glm::degrees(std::atan2(d.z, d.x));
  cam.pitch_deg = glm::degrees(std::asin(d.y));
}

// Placeholder torque curve for asset-less runs; index = rpm >> 7.
void fill_fallback_torque_curve(CarSpec* spec) {
  spec->idle_torque = 100;
  for (int i = 0; i < 104; ++i) {
    const int rpm = i << 7;
    int16_t t = 95;
    if (rpm < 2000) t = 95 + rpm / 64;
    else if (rpm < 5500) t = 110 - (rpm - 2000) / 300;
    else t = std::max(60, 104 - (rpm - 5500) / 120);
    spec->torque_curve[i] = static_cast<uint8_t>(std::clamp(static_cast<int>(t), 0, 255));
  }
}

CarSpec load_or_default_spec(const std::string& asset_dir) {
  CarSpec spec;
  if (!asset_dir.empty() && CarSpec::load(path_join(asset_dir, "CARCOUN.RES"), &spec)) {
    return spec;
  }
  S3D_WARN("CARCOUN.RES not found; using a placeholder car specification");

  // Counter-exact physical constants (verified against CARCOUN.RES) so
  // steering/grip feel right even without the retail asset file.
  spec.num_gears = 6;
  spec.car_mass = 33;
  spec.braking_eff = 200;
  spec.idle_rpm = 800;
  spec.downshift_rpm = 5000;
  spec.upshift_rpm = 7800;
  spec.max_rpm = 8250;
  const uint16_t ratios[7] = {0, 33339, 23194, 16516, 12086, 8287, 0};
  for (int i = 0; i < 7; ++i) spec.gear_ratios[i] = ratios[i];
  spec.aero_resistance = 25;
  for (int i = 0; i < 64; ++i) {
    spec.aero_resistance_table[i] = static_cast<int16_t>(
        static_cast<uint16_t>((static_cast<int32_t>(spec.aero_resistance) * i * i) >> 9));
  }
  spec.grip = 210;
  spec.sliding = 0;
  spec.surface_grip[0] = 256;  // paved
  spec.surface_grip[1] = 192;  // dirt
  spec.surface_grip[2] = 64;   // ice
  spec.surface_grip[3] = 128;  // grass
  spec.car_height = 16;
  const int16_t wheels[4][3] = {
      {-960, 0, 1984}, {960, 0, 1984}, {960, 0, -1984}, {-960, 0, -1984}};
  for (int i = 0; i < 4; ++i) {
    spec.wheel_coords[i].x = wheels[i][0];
    spec.wheel_coords[i].y = wheels[i][1];
    spec.wheel_coords[i].z = wheels[i][2];
  }
  fill_fallback_torque_curve(&spec);
  return spec;
}

// Ground callback feed to the physics: the shared grid surface model.
physics::GroundSample ground_sample(const Track& track, float wx, float wz) {
  const world::GroundSample g = world::ground_sample_at(track, wx, wz);
  physics::GroundSample out;
  out.height = g.height;
  out.surface = g.surface;
  return out;
}

// Linear interpolation between two 16-bit rotations. Per-tick deltas are small
// (well under a quarter turn), so no wraparound handling is needed.
float lerp_angle(int16_t prev, int16_t curr, float alpha) {
  return (static_cast<float>(prev) + (static_cast<float>(curr) - static_cast<float>(prev)) * alpha);
}

}  // namespace

DriveScene::DriveScene(Track track, std::string asset_dir)
    : track_(std::move(track)), asset_dir_(std::move(asset_dir)) {}

bool DriveScene::on_enter() {
  // ---- Static world geometry -----------------------------------------------
  ShapeLibrary lib;
  if (!asset_dir_.empty()) {
    lib.load_p3s(path_join(asset_dir_, "GAME1.P3S"));
    lib.load_p3s(path_join(asset_dir_, "GAME2.P3S"));
  }
  if (!lib.shapes().empty()) {
    world::TrackShapeStats stats;
    MeshBuilder mb;
    world::build_track_shape_geometry(track_, lib, mb, &stats);
    S3D_INFO("drive track: ground=%d terrain=%d elements=%d overlays=%d missing=%d",
             stats.ground_cells, stats.terrain_shapes, stats.elements, stats.overlays,
             stats.missing_shapes);
    track_mesh_ = mb.to_mesh();
  } else {
    S3D_WARN("no GAME1/GAME2 shapes found; falling back to placeholder geometry");
    track_mesh_ = build_track_mesh(track_);
  }

  // ---- Player car ----------------------------------------------------------
  spec_ = load_or_default_spec(asset_dir_);

  auto car_lib = std::make_shared<ShapeLibrary>();
  if (!asset_dir_.empty() && world::load_car_archive(asset_dir_, "COUN", car_lib.get())) {
    MeshBuilder mb;
    world::append_car_model(mb, *car_lib, glm::mat4(1.0f), kPlayerCarColor);
    car_mesh_ = mb.to_mesh();
  }
  if (car_mesh_.empty()) {
    S3D_WARN("player car shapes not found; driving invisible");
  }

  // ---- Track collision -----------------------------------------------------
  // The element plane/wall tables live in GAME.PRE. With them bound to the
  // track grid the wheels ride ramps, banked roads and elevated terrain; absent
  // the file the motion code falls back to the flat 2D ground sampler.
  if (!asset_dir_.empty() &&
      collision_.load_from_game_pre(path_join(asset_dir_, "GAME.PRE"))) {
    collision_.set_track(&track_);
    S3D_INFO("track collision loaded: %zu planes, %zu walls", collision_.plane_count(),
             collision_.wall_count());
  } else {
    S3D_WARN("GAME.PRE not found; using the flat 2D ground sampler");
  }

  reset_car();
  S3D_INFO("drive scene ready");
  return true;
}

void DriveScene::reset_car() {
  car_ = physics::CarState{};
  car_.car_transmission = physics::kTransmissionAutomatic;
  car_.car_current_gear = 1;
  car_.car_changing_gear = physics::kGearChangeInactive;
  car_.car_gearratio = spec_.gear_ratios[1];
  car_.car_gearratioshr8 = static_cast<uint16_t>(car_.car_gearratio >> 8);
  car_.car_currpm = spec_.idle_rpm;
  car_.car_sumSurfRearWheels = 1;
  car_.car_sumSurfAllWheels = 1;

  // Spawn at the start/finish cell (7,5), facing +Z (yaw 0). The physics pivot
  // rests six world units above the ground (all four wheel bottoms sit on the
  // contact plane), matching the reference pose.
  const int start_x = 7;
  const int start_y = 5;
  const physics::GroundSample g =
      ground_sample(track_, world::cell_center_x(start_x), world::cell_center_z(start_y));
  const int64_t long_scale = 64;
  car_.car_position.x = static_cast<int32_t>(world::cell_center_x(start_x)) * long_scale;
  car_.car_position.z = static_cast<int32_t>(world::cell_center_z(start_y)) * long_scale;
  car_.car_position.y = static_cast<int32_t>(g.height + kCarBodyDrop) * long_scale;
  car_.car_rotate.x = 0;  // yaw
  car_.car_rotate.y = 0;  // pitch
  car_.car_rotate.z = 0;  // roll
  car_.car_previous_position = car_.car_position;
  display_prev_ = car_;
  accumulator_ = 0.0;

  // Snap the camera behind the car so the first frame is already framed.
  const glm::vec3 car_pos = car_world_position(0.0f);
  camera_.position = car_pos + glm::vec3(0.0f, kFollowHeight, -kFollowDistance);
  aim_at(camera_, car_pos + glm::vec3(0.0f, kFollowAimHeight, 0.0f));
}

void DriveScene::physics_tick(const Controls& controls) {
  uint8_t input_flags = physics::kInputNone;
  if (controls.throttle > 0.05f) input_flags |= physics::kInputAccelerate;
  if (controls.throttle < -0.05f) input_flags |= physics::kInputBrake;
  if (controls.steer > 0.05f) input_flags |= physics::kInputSteerRight;
  if (controls.steer < -0.05f) input_flags |= physics::kInputSteerLeft;

  physics::update_car_speed(input_flags, spec_, &car_, physics::kGameFrameRateNormal);
  physics::update_player_steering(input_flags, &car_, physics::kGameFrameRateNormal);
  physics::update_grip(&car_, spec_);
  const auto sampler = [this](float wx, float wz) { return ground_sample(track_, wx, wz); };
  physics::step_car_motion(&car_, spec_, physics::kGameFrameRateNormal, sampler, &collision_);
}

void DriveScene::update_camera() {
  const glm::vec3 car_pos = car_world_position(1.0f);
  const float yaw = car_yaw(1.0f);
  // The car's nose in world space is R_y(-yaw) * +Z = (-sin, 0, cos) with the
  // corrected mat_rot_y (the old transposed matrix mirrored this on X). The
  // camera rides behind that nose.
  const glm::vec3 fwd(-std::sin(yaw), 0.0f, std::cos(yaw));
  const glm::vec3 ideal = car_pos - fwd * kFollowDistance + glm::vec3(0.0f, kFollowHeight, 0.0f);

  const glm::vec3 delta = ideal - camera_.position;
  float horizontal = std::hypot(delta.x, delta.z);
  const float h_step = std::min(horizontal, kFollowHorizontalStep);
  if (horizontal < 0.0001f) horizontal = 1.0f;
  camera_.position += glm::vec3(delta.x / horizontal * h_step,
                                std::clamp(delta.y, -kFollowVerticalStep, kFollowVerticalStep),
                                delta.z / horizontal * h_step);
  aim_at(camera_, car_pos + glm::vec3(0.0f, kFollowAimHeight, 0.0f));
}

void DriveScene::update(double dt, const Controls& controls) {
  if (controls.reset_car) reset_car();

  display_prev_ = car_;  // pose before this frame's ticks (tick N-1)
  accumulator_ += dt;
  int steps = 0;
  while (accumulator_ >= kPhysicsDt && steps < kMaxPhysicsStepsPerFrame) {
    accumulator_ -= kPhysicsDt;
    ++steps;
    physics_tick(controls);
    update_camera();
  }
}

float DriveScene::render_alpha() const {
  return std::clamp(static_cast<float>(accumulator_ / kPhysicsDt), 0.0f, 1.0f);
}

glm::vec3 DriveScene::car_world_position(float alpha) const {
  const float scale = 1.0f / 64.0f;
  const auto lerp = [alpha, scale](int32_t prev, int32_t curr) {
    return (static_cast<float>(prev) + (static_cast<float>(curr) - static_cast<float>(prev)) * alpha) *
           scale;
  };
  return glm::vec3(lerp(display_prev_.car_position.x, car_.car_position.x),
                   lerp(display_prev_.car_position.y, car_.car_position.y),
                   lerp(display_prev_.car_position.z, car_.car_position.z));
}

float DriveScene::car_yaw(float alpha) const {
  return lerp_angle(display_prev_.car_rotate.x, car_.car_rotate.x, alpha) * kAngleToRadians;
}

float DriveScene::car_pitch(float alpha) const {
  return lerp_angle(display_prev_.car_rotate.y, car_.car_rotate.y, alpha) * kAngleToRadians;
}

float DriveScene::car_roll(float alpha) const {
  return lerp_angle(display_prev_.car_rotate.z, car_.car_rotate.z, alpha) * kAngleToRadians;
}

glm::mat4 DriveScene::car_model_matrix(float alpha) const {
  // Interpolate the 0x400-unit angles first, then rebuild the physics
  // body-to-world rotation (mat_rot_zxy(-roll, -pitch, -yaw)). The matrix is
  // converted to GLM the same way the track builder's fixed_model does, so the
  // visible car is exactly the physics pose (no sign guessing at the GL layer).
  const int16_t yaw256 = static_cast<int16_t>(lerp_angle(display_prev_.car_rotate.x, car_.car_rotate.x, alpha));
  const int16_t pitch256 = static_cast<int16_t>(lerp_angle(display_prev_.car_rotate.y, car_.car_rotate.y, alpha));
  const int16_t roll256 = static_cast<int16_t>(lerp_angle(display_prev_.car_rotate.z, car_.car_rotate.z, alpha));
  const fixed::Mat3 world_rot =
      fixed::mat_rot_zxy(physics::wrap_negate(roll256), physics::wrap_negate(pitch256),
                         physics::wrap_negate(yaw256), 0);

  glm::mat4 model(1.0f);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      model[col][row] = static_cast<float>(world_rot.at(row, col)) / 16384.0f;
    }
  }
  const glm::vec3 pos = car_world_position(alpha);
  model[3][0] = pos.x;
  model[3][1] = pos.y;
  model[3][2] = pos.z;
  model = model * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -kCarBodyDrop, 0.0f));
  return model;
}

void DriveScene::render(Renderer& renderer, int width, int height) {
  renderer.draw_world(track_mesh_, camera_);
  if (!car_mesh_.empty()) {
    renderer.draw_world(car_mesh_, camera_, car_model_matrix(render_alpha()));
  }

  ++frame_index_;
  if (!screenshot_path_.empty() && frame_index_ >= screenshot_frame_) {
    capture_frame_png(screenshot_path_, width, height);
    screenshot_path_.clear();
    finished_ = true;
  }
}

void DriveScene::request_screenshot(std::string path, int frames) {
  screenshot_path_ = std::move(path);
  screenshot_frame_ = frames > 0 ? frames : 1;
}

}  // namespace s3d