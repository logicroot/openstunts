// Car motion tests: the reduced wheel-pose integrator (stateply.c port) plus
// the full engine->steering->grip->motion pipeline used by the drive scene.
// The flat sampler models a paved start/finish cell at height 1.

#include <cstdint>
#include <string>

#include "assets/trk.h"
#include "core/paths.h"
#include "physics/car_engine.h"
#include "physics/car_motion.h"
#include "physics/car_state.h"
#include "physics/car_steering.h"
#include "physics/track_collision.h"
#include "tiny_test.h"
#include "world/track_elevation.h"

using namespace s3d;
using namespace s3d::physics;

namespace {

constexpr int kStartCellX = 7;
constexpr int kStartCellY = 5;
constexpr int kCellCenterWorld = 7680;  // (7 + 0.5) * 1024
constexpr int kCellCenterLong = kCellCenterWorld << 6;
constexpr int kGroundHeight = 1;  // paved cell rides 1 unit above terrain
constexpr int kRestPivotY = (kGroundHeight + 6) << 6;  // 448 long

GroundSample flat_paved_world(float, float wz) {
  (void)wz;
  GroundSample g;
  g.height = kGroundHeight;
  g.surface = kCarSurfacePaved;
  return g;
}

CarSpec test_spec() {
  CarSpec spec;
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
  spec.idle_torque = 100;
  for (int i = 0; i < 104; ++i) spec.torque_curve[i] = 100;
  const int16_t wheels[4][3] = {
      {-960, 0, 1984}, {960, 0, 1984}, {960, 0, -1984}, {-960, 0, -1984}};
  for (int i = 0; i < 4; ++i) {
    spec.wheel_coords[i].x = wheels[i][0];
    spec.wheel_coords[i].y = wheels[i][1];
    spec.wheel_coords[i].z = wheels[i][2];
  }
  return spec;
}

CarState parked_car(const CarSpec& spec) {
  CarState car;
  car.car_transmission = kTransmissionAutomatic;
  car.car_current_gear = 1;
  car.car_changing_gear = kGearChangeInactive;
  car.car_gearratio = spec.gear_ratios[1];
  car.car_gearratioshr8 = car.car_gearratio >> 8;
  car.car_currpm = spec.idle_rpm;
  car.car_sumSurfRearWheels = 1;
  car.car_sumSurfAllWheels = 1;
  car.car_position.x = kCellCenterLong;
  car.car_position.z = (kStartCellY << 10) + (512 << 6);
  car.car_position.y = kRestPivotY;
  return car;
}

// One 20 Hz tick of the full drive-scene pipeline.
void pipeline_tick(CarState* car, const CarSpec& spec, uint8_t flags) {
  update_car_speed(flags, spec, car, kGameFrameRateNormal);
  update_player_steering(flags, car, kGameFrameRateNormal);
  update_grip(car, spec);
  step_car_motion(car, spec, kGameFrameRateNormal, flat_paved_world);
}

// The stored yaw is an unsigned 0..0x3FF circle; right turns accumulate
// negative and wrap through 0x3FF. Interpret values above 0x200 (180 degrees)
// as the corresponding negative heading.
int16_t signed_yaw(const CarState& car) {
  const int16_t yaw = car.car_rotate.x;
  return yaw > 0x200 ? static_cast<int16_t>(yaw - 0x400) : yaw;
}

}  // namespace

TEST(motion_rest_position_is_stable) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_paved_world);
  // Four wheels rest exactly on the paved surface; the pivot stays at +6.
  EXPECT_EQ(car.car_position.y, kRestPivotY);
  EXPECT_EQ(car.car_rotate.x, 0);  // yaw
  for (int i = 0; i < kCarWheelCount; ++i) {
    EXPECT_EQ(car.car_surfaceWhl[i], kCarSurfacePaved);
  }
  EXPECT_EQ(car.car_sumSurfAllWheels, 4);
}

TEST(motion_forward_travel_moves_along_plus_z) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  car.car_actual_speed = 4096;

  const int32_t previous_z = car.car_position.z;
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_paved_world);

  // travel = 4096 * 1408 / 15360 = 375 world units per tick. The travel word
  // is added directly to the long (1/64 world) position, like the original
  // wheel_forward_travel add -- no extra 64x scaling here.
  EXPECT_EQ(car.car_position.z - previous_z, 375);
  EXPECT_EQ(car.car_position.x, kCellCenterLong);
  EXPECT_EQ(car.car_rotate.x, 0);
}

TEST(motion_acceleration_drives_forward) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  const int32_t start_z = car.car_position.z;
  for (int i = 0; i < 30; ++i) {
    pipeline_tick(&car, spec, kInputAccelerate);
  }
  EXPECT_TRUE(car.car_position.z > start_z);
  EXPECT_EQ(car.car_rotate.x, 0);  // no steering -> no yaw
  EXPECT_TRUE(car.car_position.y >= kRestPivotY - 64);
}

TEST(motion_steering_right_turns_yaw_negative) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  for (int i = 0; i < 60; ++i) {
    pipeline_tick(&car, spec, kInputAccelerate | kInputSteerRight);
  }
  // Positive steering response -> positive front-wheel heading (restunts
  // test-phantom-motion: +55 steering -> +13 RIGHT heading). The corrected
  // mat_rot_y rotates that heading toward +X in the car frame, so a right
  // turn drives the nose and the position toward +X and yaw goes negative.
  EXPECT_TRUE(signed_yaw(car) < 0);
  EXPECT_TRUE(car.car_actual_speed > 0);
  EXPECT_TRUE(car.car_position.x > kCellCenterLong);
}

TEST(motion_steering_left_turns_yaw_positive) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  for (int i = 0; i < 60; ++i) {
    pipeline_tick(&car, spec, kInputAccelerate | kInputSteerLeft);
  }
  // Mirror image of the right-turn assertions: negative heading -> yaw grows
  // positive and the car drifts toward -X.
  EXPECT_TRUE(signed_yaw(car) > 0);
  EXPECT_TRUE(car.car_actual_speed > 0);
  EXPECT_TRUE(car.car_position.x < kCellCenterLong);
}

TEST(motion_car_never_sinks_through_road) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  for (int i = 0; i < 120; ++i) {
    pipeline_tick(&car, spec, kInputAccelerate);
  }
  // Pivot rests exactly at ground+6; allow one tick of spring slack.
  EXPECT_TRUE(car.car_position.y >= kRestPivotY - 64);
}

// ---------------------------------------------------------------------------
// Track-collision-aware wheels. When a loaded TrackCollision with a bound
// track is passed, the wheels ride the element planes instead of the flat 2D
// sampler. These skip when the Stunts assets are unavailable.
// ---------------------------------------------------------------------------
namespace {

constexpr int kElevatedHeight = 450;

bool load_collision(TrackCollision* tc) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return false;
  return tc->load_from_game_pre(path_join(dir, "GAME.PRE"));
}

void fill_terrain(Track* t, uint8_t terrain) {
  for (uint8_t& byte : t->terrain) byte = terrain;
}

}  // namespace

TEST(motion_track_collision_uses_track_surface) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track track;  // empty: no elements, plain grass
  tc.set_track(&track);

  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  car.car_position.y = 6 << 6;  // rest on the flat plane at y = 0
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_paved_world, &tc);

  // The collision model reports grass, not the flat sampler's paved surface.
  for (int i = 0; i < kCarWheelCount; ++i) {
    EXPECT_EQ(car.car_surfaceWhl[i], kCarSurfaceGrass);
  }
  EXPECT_EQ(car.car_sumSurfAllWheels, 4);
  // Grass has a 1-unit checkerboard, so the pivot stays within [6,7] + 1.
  EXPECT_TRUE(car.car_position.y >= (6 << 6) && car.car_position.y <= (8 << 6));
}

TEST(motion_track_collision_elevated_terrain_lifts_car) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track track;
  fill_terrain(&track, 6);  // whole track is an elevated plateau (+450)
  tc.set_track(&track);

  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  car.car_position.y = (kElevatedHeight + 6) << 6;
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_paved_world, &tc);

  // The plateau lifts the car ~450 units above the flat sampler's rest height.
  // (Grass adds a 1-unit checkerboard, so the exact pivot varies by a few
  // units; the flat sampler would have left it at kRestPivotY = 7 << 6.)
  EXPECT_TRUE(car.car_position.y >= kRestPivotY + 400);
  EXPECT_TRUE(car.car_position.y >= (kElevatedHeight + 5) << 6);
  EXPECT_TRUE(car.car_position.y <= (kElevatedHeight + 8) << 6);
  EXPECT_EQ(car.car_sumSurfAllWheels, 4);
  for (int i = 0; i < kCarWheelCount; ++i) {
    EXPECT_TRUE(car.car_surfaceWhl[i] != kWheelContactNone);
  }
}

TEST(motion_track_collision_unbound_falls_back_to_sampler) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  // No set_track: the model has no element grid, so motion must fall back to
  // the caller's ground sampler (paved at height 1).
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_paved_world, &tc);

  EXPECT_EQ(car.car_position.y, kRestPivotY);
  for (int i = 0; i < kCarWheelCount; ++i) {
    EXPECT_EQ(car.car_surfaceWhl[i], kCarSurfacePaved);
  }
}

TEST(motion_track_collision_flat_plane_travel_matches) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track track;
  tc.set_track(&track);

  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  car.car_position.y = 6 << 6;
  car.car_actual_speed = 4096;

  const int32_t previous_z = car.car_position.z;
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_paved_world, &tc);

  // Plane 0 is flat, so the plane-aware travel must match the no-plane branch:
  // 4096 * 1408 / 15360 = 375 world units along +z.
  EXPECT_EQ(car.car_position.z - previous_z, 375);
  EXPECT_EQ(car.car_position.x, kCellCenterLong);
  EXPECT_EQ(car.car_rotate.x, 0);
}

TEST(motion_track_collision_drives_real_track_without_sinking) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir)\n");
    return;
  }
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no GAME.PRE)\n");
    return;
  }
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));
  tc.set_track(&track);

  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  car.car_position.x = (7 * 1024 + 512) << 6;
  car.car_position.z = (5 * 1024 + 512) << 6;
  car.car_position.y = (1 + 6) << 6;

  const auto sampler = [&track](float wx, float wz) {
    const world::GroundSample g = world::ground_sample_at(track, wx, wz);
    GroundSample out;
    out.height = g.height;
    out.surface = g.surface;
    return out;
  };

  int ground_ticks = 0;
  for (int i = 0; i < 400; ++i) {
    const uint8_t flags =
        kInputAccelerate | (((i / 40) % 2) ? kInputSteerLeft : kInputSteerRight);
    update_car_speed(flags, spec, &car, kGameFrameRateNormal);
    update_player_steering(flags, &car, kGameFrameRateNormal);
    update_grip(&car, spec);
    step_car_motion(&car, spec, kGameFrameRateNormal, sampler, &tc);

    // The plane push-out must keep the car on the world floor; a wild value
    // signals an unstable plane transition.
    EXPECT_TRUE(car.car_position.y >= 0);
    EXPECT_TRUE(car.car_position.y < (800 << 6));
    if (car.car_sumSurfAllWheels > 0) ++ground_ticks;
  }
  EXPECT_TRUE(ground_ticks > 200);
}