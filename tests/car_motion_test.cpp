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

GroundSample flat_water_world(float, float wz) {
  (void)wz;
  GroundSample g;
  g.height = kGroundHeight;
  g.surface = kCarSurfaceWater;
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
  apply_crash_control(*car, &flags);
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

// Water crash (stateply.c loc_15163): every wheel on surface 5 (water) puts the
// car in the sinking crash state; a healthy car on paved does not, and an
// existing crash flag is never overwritten by the water check.
TEST(motion_all_wheels_on_water_enters_sinking_state) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  EXPECT_EQ(car.car_crashBmpFlag, kCrashEventNone);
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_water_world);
  for (int i = 0; i < kCarWheelCount; ++i) {
    EXPECT_EQ(car.car_surfaceWhl[i], kCarSurfaceWater);
  }
  EXPECT_EQ(car.car_crashBmpFlag, kCrashEventWater);
}

TEST(motion_paved_does_not_enter_sinking_state) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_paved_world);
  EXPECT_EQ(car.car_crashBmpFlag, kCrashEventNone);
}

TEST(motion_water_does_not_override_existing_crash) {
  CarSpec spec = test_spec();
  CarState car = parked_car(spec);
  car.car_crashBmpFlag = kCrashEventCollision;
  step_car_motion(&car, spec, kGameFrameRateNormal, flat_water_world);
  EXPECT_EQ(car.car_crashBmpFlag, kCrashEventCollision);
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
    uint8_t flags = kInputAccelerate | (((i / 40) % 2) ? kInputSteerLeft : kInputSteerRight);
    apply_crash_control(car, &flags);
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
  // Circling this track must sweep at least one wheel across a wall (integration
  // check that step_car_motion reaches resolve_wall_contact). The latch is
  // sticky, so it stays set once the wall response fires.
  EXPECT_EQ(car.car_collision_latch, kCarCollisionLatchSet);
  // At least one of those wall hits is hard enough to enter the crash state.
  EXPECT_EQ(car.car_crashBmpFlag, kCrashEventCollision);
}

// ---------------------------------------------------------------------------
// resolve_wall_contact (stateply.c wall block)
//
// A wall with orientation 0 through the origin puts the wall line on the world
// z axis, so the rotated across-wall coordinate `c.z` equals the wheel's world
// x. The wheel crosses the wall when its x flips sign.
// ---------------------------------------------------------------------------
namespace {

CollisionSample flat_wall() {
  CollisionSample s;
  s.plane_index = 0;
  s.wall_index = 0x65;
  s.wall_start_x = 0;
  s.wall_start_z = 0;
  s.wall_orientation = 0;
  s.wall_height = 0x2A;        // 42
  s.wall_lower_bound = -12;
  return s;
}

}  // namespace

TEST(wall_contact_no_hit_when_both_ends_same_side) {
  const CollisionSample wall = flat_wall();
  const WallContact c =
      resolve_wall_contact(wall, 0, fixed::Vec3{100, 0, 500}, fixed::Vec3{95, 0, 500}, 375, 0, 0, 0);
  EXPECT_TRUE(!(c.hit));
}

TEST(wall_contact_no_hit_outside_height_band) {
  const CollisionSample wall = flat_wall();
  const fixed::Vec3 pre{3, 0, 500};
  const fixed::Vec3 post{-2, 0, 500};
  // plane_distance above wall_height (42) or at/below wall_lower_bound (-12).
  EXPECT_TRUE(!(resolve_wall_contact(wall, 100, pre, post, 375, 0, 0, 0).hit));
  EXPECT_TRUE(!(resolve_wall_contact(wall, -20, pre, post, 375, 0, 0, 0).hit));
  EXPECT_TRUE(!(resolve_wall_contact(wall, 42, pre, post, 375, 0, 0, 0).hit));
}

TEST(wall_contact_crossing_blocks_part_of_travel) {
  const CollisionSample wall = flat_wall();
  const WallContact c =
      resolve_wall_contact(wall, 0, fixed::Vec3{3, 0, 500}, fixed::Vec3{-2, 0, 500}, 375, 0, 0, 0);
  EXPECT_TRUE(c.hit);
  // Penetration = |post.x| << 6 = 2 * 64 = 128 long units; travel 375 survives
  // the remainder.
  EXPECT_EQ(c.remaining_travel, 375 - 128);
  // The car is nudged along the wall: +768 across, +penetration along.
  EXPECT_EQ(c.deflection.x, 768);
  EXPECT_EQ(c.deflection.z, 128);
}

TEST(wall_contact_full_crossing_kills_travel) {
  const CollisionSample wall = flat_wall();
  const WallContact c =
      resolve_wall_contact(wall, 0, fixed::Vec3{50, 0, 500}, fixed::Vec3{-50, 0, 500}, 375, 0, 0, 0);
  EXPECT_TRUE(c.hit);
  // The wheel jumped far past the wall line: no travel remains.
  EXPECT_TRUE(c.remaining_travel <= 0);
}

TEST(wall_contact_swaps_endpoints_and_mirrors_slide) {
  const CollisionSample wall = flat_wall();
  // post is the upper end (x = +3) now, so the endpoints swap and the along-wall
  // nudge is mirrored.
  const WallContact c =
      resolve_wall_contact(wall, 0, fixed::Vec3{-2, 0, 500}, fixed::Vec3{3, 0, 500}, 375, 0, 0, 0);
  EXPECT_TRUE(c.hit);
  EXPECT_EQ(c.remaining_travel, 375 - 128);
  EXPECT_EQ(c.deflection.x, -768);
  EXPECT_EQ(c.deflection.z, 128);
}

TEST(wall_contact_ignores_zero_travel_and_missing_wall) {
  CollisionSample wall = flat_wall();
  const fixed::Vec3 pre{3, 0, 500};
  const fixed::Vec3 post{-2, 0, 500};
  EXPECT_TRUE(!(resolve_wall_contact(wall, 0, pre, post, 0, 0, 0, 0).hit));
  wall.wall_index = -1;
  EXPECT_TRUE(!(resolve_wall_contact(wall, 0, pre, post, 375, 0, 0, 0).hit));
}

// ---------------------------------------------------------------------------
// compute_wall_crash (stateply.c loc_1546E..loc_154FA)
//
// si folds the wall/car heading difference into [0, 0x100]; var_190 packs
// ((si * 0x46) sar 8 - 0x64) negated into the high byte (a 16-bit `imul` that
// wraps). A hit crashes when car_actual_speed > var_190.
// ---------------------------------------------------------------------------

TEST(wall_crash_below_threshold_does_not_trigger) {
  // yaw 0x300 vs wall-frame 0 -> si = 0x100, var_138 = 0, threshold = 30 << 8.
  const WallCrash c = compute_wall_crash(0x300, 0, 30 << 8);
  EXPECT_TRUE(!c.triggered);
  EXPECT_EQ(c.heading_kick, 0);
}

TEST(wall_crash_above_threshold_kicks_heading) {
  const WallCrash c = compute_wall_crash(0x300, 0, (30 << 8) + 1);
  EXPECT_TRUE(c.triggered);
  // var_138 == 0 branch: kick = +(si << 1) = +0x200.
  EXPECT_EQ(c.heading_kick, 512);
}

TEST(wall_crash_mirrored_branch_negates_kick) {
  // yaw 0 vs wall-frame 0x100 also folds to si = 0x100, but from the far side
  // (var_138 = 1), so the kick is negated.
  const WallCrash c = compute_wall_crash(0, 0x100, (30 << 8) + 1);
  EXPECT_TRUE(c.triggered);
  EXPECT_EQ(c.heading_kick, -512);
}

TEST(wall_crash_reproduces_16bit_multiply_wrap) {
  // yaw 0 vs wall-frame 0x200 folds to si = 0x200. 0x200 * 0x46 = 0x8C00, whose
  // signed value is negative; sar 8 gives -0x74, so var_190 = 0xD8 << 8.
  const uint16_t threshold = static_cast<uint16_t>(0xD8 << 8);
  EXPECT_TRUE(!compute_wall_crash(0, 0x200, threshold).triggered);
  const WallCrash c = compute_wall_crash(0, 0x200, static_cast<uint16_t>(threshold + 1));
  EXPECT_TRUE(c.triggered);
  EXPECT_EQ(c.heading_kick, -1024);
}

// ---------------------------------------------------------------------------
// apply_crash_control (state.c player_op)
// ---------------------------------------------------------------------------

TEST(crash_control_forces_brake_until_recovers) {
  CarState car;
  uint8_t flags = kInputAccelerate | kInputSteerRight;
  apply_crash_control(car, &flags);
  EXPECT_EQ(flags, kInputAccelerate | kInputSteerRight);  // untouched while healthy

  car.car_crashBmpFlag = kCrashEventCollision;
  flags = kInputAccelerate | kInputSteerLeft;
  apply_crash_control(car, &flags);
  EXPECT_EQ(flags, kInputBrake);
}