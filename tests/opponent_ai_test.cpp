// Opponent AI tests: the seg001.asm `opponent_op` port (src/physics/opponent_ai).
// The route side uses the synthetic ring from synthetic_track.h (start cell
// (col 3, bottom-up row 2), start/finish direction 0) and a default OpponentData
// (all-zero sped table), so the tests need no game assets.

#include <cstdint>
#include <vector>

#include "assets/car.h"
#include "assets/opponent.h"
#include "physics/car_motion.h"
#include "physics/car_state.h"
#include "physics/opponent_ai.h"
#include "synthetic_track.h"
#include "tiny_test.h"
#include "world/track_route.h"

using namespace s3d;
using namespace s3d::physics;
using namespace s3d::world;
using namespace s3d_test;

namespace {

// A capable-enough SIMD profile for the physics tail (update_car_speed /
// update_grip / step_car_motion), mirroring car_motion_test's test_spec().
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

// A car resting on flat height-0 ground, in gear 1, ready to be driven. The
// pivot rests at ground + 6 (the suspension float height).
CarState resting_opp(const CarSpec& spec) {
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
  car.car_position.y = 6 << 6;  // world y = 6 (ground 0 + 6)
  car.car_position.x = 0;
  car.car_position.z = 0;
  return car;
}

// Flat ground at height 0.
GroundSample flat_ground(float, float) {
  GroundSample g;
  g.height = 0;
  g.surface = kCarSurfacePaved;
  return g;
}

// The synthetic ring route plus the opponent's course path (all-zero sped:
// every element weighs 1, so the path follows the ring).
struct AiFixture {
  Track track = make_loop_track();
  TrackRoute route;
  OpponentData data;  // all-zero sped table (engine_torque = 0x64)
  std::vector<uint16_t> path;

  AiFixture() {
    route.build(track, 3, 2, 0x000);
    path = route.opponent_path(data.speed_table().data(), OpponentData::kSpeedCount);
  }
};

OpponentRaceSetup make_setup(const AiFixture& fx) {
  OpponentRaceSetup s;
  s.route = &fx.route;
  s.track = &fx.track;
  s.data = &fx.data;
  s.path = fx.path;
  s.start_col = 3;
  s.start_row = 2;
  s.track_angle = 0;
  s.game_inputmode = 0;
  return s;
}

}  // namespace

TEST(route_start_angle_is_stored) {
  const Track t = make_loop_track();
  TrackRoute route;
  EXPECT_TRUE(route.build(t, 3, 2, 0x200));
  EXPECT_EQ(route.start_col(), 3);
  EXPECT_EQ(route.start_row(), 2);
  EXPECT_EQ(route.start_angle(), 0x200);
}

TEST(ai_init_writes_first_checkpoint) {
  AiFixture fx;
  const auto setup = make_setup(fx);
  CarState opp;
  OpponentAiState ai;
  init_opponent_checkpoint(setup, &ai, &opp);

  // The same sub_18D60( node 0, field_CE 0 ) the race setup issues. The return
  // is the "last arrow" flag (false for the start straight's first arrow), so
  // just fetch the checkpoint the init helper used.
  RouteCheckpoint expected;
  fx.route.checkpoint(fx.path[0], 0, fx.track, &expected);
  EXPECT_TRUE(expected.x != 0 || expected.z != 0);

  EXPECT_EQ(opp.car_route_target.x, static_cast<int16_t>(expected.x));
  EXPECT_EQ(opp.car_route_target.y, static_cast<int16_t>(expected.y));
  EXPECT_EQ(opp.car_route_target.z, static_cast<int16_t>(expected.z));
  EXPECT_EQ(opp.car_route_first_edge.x, static_cast<int16_t>(expected.ax));
  EXPECT_EQ(opp.car_route_first_edge.y, static_cast<int16_t>(expected.ay));
  EXPECT_EQ(opp.car_route_first_edge.z, static_cast<int16_t>(expected.az));
  EXPECT_EQ(opp.car_route_second_edge.x, static_cast<int16_t>(expected.bx));
  EXPECT_EQ(opp.car_route_second_edge.y, static_cast<int16_t>(expected.by));
  EXPECT_EQ(opp.car_route_second_edge.z, static_cast<int16_t>(expected.bz));
  EXPECT_EQ(opp.car_route_has_reverse_path, static_cast<int16_t>(expected.opp1));
  EXPECT_EQ(ai.target_speed_code,
            fx.data.speed_for(expected.speed_code, expected.surface));
  // The post-call field_CE++ leaves the car on course arrow 1.
  EXPECT_EQ(opp.car_route_point_index, 1);
  EXPECT_EQ(opp.car_route_index, 0);
  EXPECT_EQ(opp.car_lap_count, 0);
}

// A checkpoint dead ahead and 256 units to the right must turn the wheel one
// 8/20 fps step to the right (loc_14B62 with var_14 = 8).
TEST(ai_steers_stepwise_toward_checkpoint) {
  AiFixture fx;
  const auto setup = make_setup(fx);
  CarState opp = resting_opp(test_spec());
  opp.car_velocity_heading_offset = 1;  // spun wheel: the clamp stays armed
  opp.car_route_target.x = 256;
  opp.car_route_target.y = 0;
  opp.car_route_target.z = 0;
  // Player far behind: outside the 90-degree interception box, aim directly.
  CarState player = resting_opp(test_spec());
  player.car_position.z = -2000 * 64;  // far behind: outside the intercept box

  OpponentAiState ai;
  ai.target_speed_code = 0;
  const uint8_t ret =
      run_opponent_ai(setup, &ai, &opp, player, test_spec(), kGameFrameRateNormal,
                      flat_ground, nullptr);

  // |heading| > 0x41 so var_2C clamps to 0x41; diff 0x41 -> step by var_14 = 8.
  EXPECT_EQ(opp.car_steeringAngle, 8);
  EXPECT_EQ(ret, 0);  // no player interception
}

// Parked on the init checkpoint (distance < 0xC8), the AI advances to course
// arrow 1 (the last arrow of the start straight): node 0 -> node 1, field_CE 0.
TEST(ai_advances_checkpoint_when_close) {
  AiFixture fx;
  const auto setup = make_setup(fx);
  RouteCheckpoint cp;
  fx.route.checkpoint(fx.path[0], 0, fx.track, &cp);
  EXPECT_TRUE(cp.x != 0 || cp.z != 0);

  CarState opp = resting_opp(test_spec());
  opp.car_position.x = static_cast<int32_t>(cp.x) << 6;
  opp.car_position.z = static_cast<int32_t>(cp.z) << 6;
  opp.car_velocity_heading_offset = 1;  // no steering retry-advance
  CarState player = resting_opp(test_spec());
  player.car_position.z = -4000 * 64;

  OpponentAiState ai;
  init_opponent_checkpoint(setup, &ai, &opp);
  run_opponent_ai(setup, &ai, &opp, player, test_spec(), kGameFrameRateNormal,
                  flat_ground, nullptr);

  // Element 1 (start/finish straight) has 2 arrows: after the init wrote arrow
  // 0 and field_CE = 1, this tick's advance fetched arrow 1 (the last) and
  // rolled the node index to the first corner.
  EXPECT_EQ(opp.car_route_index, 1);
  EXPECT_EQ(opp.car_route_point_index, 0);
  EXPECT_EQ(opp.car_lap_count, 0);
}

// An un-crashed player just ahead of a lap-1+ opponent (parked past the start
// line) selects the first-edge interception and the off-track band crashes the
// opponent.
TEST(ai_player_ahead_sets_intercept_then_offtrack_crash) {
  AiFixture fx;
  const auto setup = make_setup(fx);
  const CarSpec spec = test_spec();

  CarState opp = resting_opp(spec);
  opp.car_position.z = static_cast<int32_t>(0xA80) << 6;  // past row-2 centre (0xA00)
  opp.car_velocity_heading_offset = 1;
  opp.car_lap_count = 1;  // the off-track band only applies from lap one

  CarState player = resting_opp(spec);
  player.car_position.z = static_cast<int32_t>(0xA80 + 100) << 6;  // just ahead

  OpponentAiState ai;
  init_opponent_checkpoint(setup, &ai, &opp);
  // Craft a checkpoint straight below the start-line row so the interception
  // side is fully deterministic (overriding the real node-0 checkpoint).
  opp.car_route_target.x = 0;
  opp.car_route_target.y = 0;
  opp.car_route_target.z = 0;
  opp.car_route_first_edge.x = 0;
  opp.car_route_first_edge.y = 0;
  opp.car_route_first_edge.z = 0;
  const uint8_t ret =
      run_opponent_ai(setup, &ai, &opp, player, spec, kGameFrameRateNormal, flat_ground,
                      nullptr);

  EXPECT_EQ(ret, 1);  // first-edge interception (the player is on that side)
  EXPECT_EQ(opp.car_crashBmpFlag, kCrashEventOffTrack);
}