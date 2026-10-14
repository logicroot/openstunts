// Ramp jump regression test.
//
// A car driving straight up a ramp tile must follow the slope (no spurious wall
// crash) with its nose pitched up. This was written for the two original-math
// fixes it guards end to end:
//   * the sign of the pitch rotation (fixed::mat_rot_x, seg024.asm), and
//   * the column-major plane rotation loaded from GAME.PRE (math.c MATRIX).
// Before those fixes a fast car "collided with the ground" at the ramp apex
// while a slow car passed, which is exactly the reported bug.
//
// Needs the shipped GAME.PRE plan/wall tables, so it skips without assets.

#include <cstdint>
#include <string>

#include "assets/car.h"
#include "assets/trk.h"
#include "core/paths.h"
#include "physics/car_motion.h"
#include "physics/track_collision.h"
#include "tiny_test.h"

using namespace s3d;
using namespace s3d::physics;

namespace {

constexpr int kRampColumn = 5;
// Column 5, rows 5..9: road, road, ramp (item 38, rot 0, physical model 16),
// road, road. The car starts on the first row heading +z.
constexpr uint8_t kApproach[5] = {4, 4, 38, 4, 4};

// Hand-built spec/state so the test stays asset-independent apart from
// GAME.PRE (same fixture shape as car_motion_test's).
CarSpec ramp_spec() {
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
  spec.surface_grip[0] = 256;
  spec.surface_grip[1] = 192;
  spec.surface_grip[2] = 64;
  spec.surface_grip[3] = 128;
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

CarState ramp_car(const CarSpec& spec, int col, int row) {
  CarState car;
  car.car_transmission = kTransmissionAutomatic;
  car.car_current_gear = 1;
  car.car_changing_gear = kGearChangeInactive;
  car.car_gearratio = spec.gear_ratios[1];
  car.car_gearratioshr8 = static_cast<uint16_t>(car.car_gearratio >> 8);
  car.car_currpm = spec.idle_rpm;
  car.car_sumSurfFrontWheels = 1;
  car.car_sumSurfRearWheels = 1;
  car.car_sumSurfAllWheels = 2;
  car.car_rotate.x = 0;  // facing +z
  car.car_position.x = (col * 1024 + 512) * 64;
  car.car_position.z = (row * 1024 + 512) * 64;
  car.car_position.y = 6 << 6;
  car.car_previous_position = car.car_position;
  return car;
}

GroundSample flat_paved(float, float) {
  GroundSample g;
  g.height = 0;
  g.surface = kCarSurfacePaved;
  return g;
}

bool build_scene(Track* track, TrackCollision* tc) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return false;
  for (int i = 0; i < 5; ++i) {
    track->items[static_cast<size_t>(5 + i) * Track::kSize + kRampColumn] = kApproach[i];
  }
  if (!tc->load_from_game_pre(path_join(dir, "GAME.PRE"))) return false;
  tc->set_track(track);
  return true;
}

struct JumpResult {
  int crash_frame = -1;
  int max_y = 0;
  int max_front_minus_rear = -0x4000;  // pitch evidence while on the slope
};

// Runs one fixed-speed approach and reports what happened. 20 Hz physics,
// matching the drive scene.
JumpResult drive_ramp(uint16_t speed, int frames) {
  Track track;
  TrackCollision tc;
  JumpResult r;
  if (!build_scene(&track, &tc)) return r;

  const CarSpec spec = ramp_spec();
  CarState car = ramp_car(spec, kRampColumn, 5);
  car.car_actual_speed = speed;
  r.max_y = car.car_position.y;

  for (int f = 0; f < frames; ++f) {
    car.car_actual_speed = speed;
    step_car_motion(&car, spec, 20, flat_paved, &tc);
    if (car.car_position.y > r.max_y) r.max_y = car.car_position.y;
    const int front = car.car_wheel_contact_positions[0].y;
    const int rear = car.car_wheel_contact_positions[2].y;
    if (front - rear > r.max_front_minus_rear) r.max_front_minus_rear = front - rear;
    if (car.car_crashBmpFlag != 0) {
      r.crash_frame = f;
      break;
    }
  }
  return r;
}

}  // namespace

TEST(ramp_jump_slow_climbs_the_slope) {
  Track track;
  TrackCollision tc;
  if (!build_scene(&track, &tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  // Slow enough that the car needs most of a full ramp traverse to reach it.
  const JumpResult r = drive_ramp(4000, 520);
  EXPECT_EQ(r.crash_frame, -1);
  // On the way up the nose points up: the front contacts ride higher than the
  // rear ones (guards the pitch-rotation sign).
  EXPECT_TRUE(r.max_front_minus_rear > 0);
}

TEST(ramp_jump_fast_does_not_crash) {
  Track track;
  TrackCollision tc;
  if (!build_scene(&track, &tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  for (uint16_t speed : {8000, 12000, 16000}) {
    const JumpResult r = drive_ramp(speed, 160);
    // The reported bug: a fast car "collided with the ground" before the jump.
    EXPECT_EQ(r.crash_frame, -1);
    // It should climb the slope / launch rather than stall at the base.
    EXPECT_TRUE(r.max_y > 5000);
  }
}
