// Steering response tests: ported from restunts opponent.c
// (update_player_steering). These pin the response-table indexing, the
// fourfold away-from-center response, the centering limit and the deadzone.

#include <cstdint>

#include "assets/trk.h"
#include "physics/car_state.h"
#include "physics/car_steering.h"
#include "tiny_test.h"

using namespace s3d;
using namespace s3d::physics;

namespace {

CarState make_car(uint16_t speed) {
  CarState car;
  car.car_actual_speed = speed;
  car.car_transmission = kTransmissionAutomatic;
  car.car_current_gear = 1;
  car.car_steeringAngle = 0;
  return car;
}

}  // namespace

TEST(steering_right_input_reads_response_table) {
  // speed 5000 -> speed_index 4 (row 1) -> +7 at 20 Hz.
  CarState car = make_car(5000);
  update_player_steering(kInputSteerRight, &car, kGameFrameRateNormal);
  EXPECT_EQ(car.car_steeringAngle, 7);
}

TEST(steering_left_input_is_opposite_sign) {
  CarState car = make_car(5000);
  update_player_steering(kInputSteerLeft, &car, kGameFrameRateNormal);
  EXPECT_EQ(car.car_steeringAngle, -7);
}

TEST(steering_low_rate_response_is_double) {
  // 10 Hz response for row 1 is +14.
  CarState car = make_car(5000);
  update_player_steering(kInputSteerRight, &car, kGameFrameRateLow);
  EXPECT_EQ(car.car_steeringAngle, 14);
}

TEST(steering_away_from_center_quadruples_response) {
  CarState car = make_car(5000);
  car.car_steeringAngle = -5;  // already turning the other way
  update_player_steering(kInputSteerRight, &car, kGameFrameRateNormal);
  // 7 << 2 = 28, then wraps the stored angle: -5 + 28 = 23.
  EXPECT_EQ(car.car_steeringAngle, 23);
}

TEST(steering_response_clamps_at_limit) {
  // Row 0 at 20 Hz gives +8 per tick; 40 ticks reach the 240 clamp.
  CarState car = make_car(500);
  for (int i = 0; i < 40; ++i) {
    update_player_steering(kInputSteerRight, &car, kGameFrameRateNormal);
  }
  EXPECT_EQ(car.car_steeringAngle, 240);
  update_player_steering(kInputSteerRight, &car, kGameFrameRateNormal);
  EXPECT_EQ(car.car_steeringAngle, 240);
}

TEST(steering_recenters_toward_center_at_limited_rate) {
  // No input: response table centre-of-range sample 7 << 1 = 14 caps the
  // centering step, so 200 -> 186 in one tick (not down to 0).
  CarState car = make_car(5000);
  car.car_steeringAngle = 200;
  update_player_steering(kInputNone, &car, kGameFrameRateNormal);
  EXPECT_EQ(car.car_steeringAngle, 186);
}

TEST(steering_deadzone_clears_small_angles) {
  CarState car = make_car(5000);
  car.car_steeringAngle = 6;
  update_player_steering(kInputNone, &car, kGameFrameRateNormal);
  // Centering returns to exactly 0, then the deadzone keeps it there.
  EXPECT_EQ(car.car_steeringAngle, 0);
}

TEST(steering_stopped_car_keeps_lock) {
  // No speed, no input: nothing happens.
  CarState car = make_car(0);
  car.car_steeringAngle = 60;
  update_player_steering(kInputNone, &car, kGameFrameRateNormal);
  EXPECT_EQ(car.car_steeringAngle, 60);
}

// ---- Cornering heading (restunts test-phantom-motion.c) -------------------
// The reference calls update_grip then returns
// scale_cornering_angle(front_wheel_response_angle, 2). These constants pin
// the arithmetic-shift (SAR) semantics of scale_cornering_angle: a logical
// shift would turn the left results (e.g. -14) into huge positive words.

namespace {

// Same grip geometry as the reference helper: grip 500, all surface grips 256.
CarSpec cornering_spec() {
  CarSpec spec;
  spec.grip = 500;
  spec.sliding = 256;
  spec.surface_grip[0] = 256;
  spec.surface_grip[1] = 256;
  spec.surface_grip[2] = 256;
  spec.surface_grip[3] = 256;
  return spec;
}

int16_t cornering_heading(int16_t steering_angle, uint16_t speed) {
  CarSpec spec = cornering_spec();
  CarState car;
  car.car_sumSurfAllWheels = kCarWheelCount;
  car.car_steeringAngle = steering_angle;
  car.car_actual_speed = speed;
  car.car_rev_speed = speed;
  for (int i = 0; i < kCarWheelCount; ++i) car.car_surfaceWhl[i] = kCarSurfacePaved;
  update_grip(&car, spec);
  return scale_cornering_angle(car.car_front_wheel_response_angle, 2);
}

}  // namespace

TEST(cornering_safe_steering_headings_match_reference) {
  // +55 steering -> +13 right heading; -55 -> -14 left heading (the leftover
  // corner bias); both from restunts test_original_cornering_bias.
  EXPECT_EQ(cornering_heading(55, 25600), 13);
  EXPECT_EQ(cornering_heading(-55, 25600), -14);
}

TEST(cornering_sliding_steering_headings_match_reference) {
  // Once the grip demand exceeds the combined grip, the slide blend applies.
  EXPECT_EQ(cornering_heading(56, 25600), 8);
  EXPECT_EQ(cornering_heading(-56, 25600), -9);
}

// The grip word the engine reads for each surface id. Slots 1..4 are
// surfacegrip[0..3]; slot 5 (water) is the struct word that follows
// surfacegrip[3], not a wrap-around to the grass value.
TEST(grip_value_for_surface_reads_sliding_values_slots) {
  CarSpec spec;
  spec.sliding = 7;
  spec.surface_grip[0] = 256;
  spec.surface_grip[1] = 192;
  spec.surface_grip[2] = 64;
  spec.surface_grip[3] = 128;
  spec.sliding_water = 0;

  EXPECT_EQ(grip_value_for_surface(0, spec), 7);    // airborne: sliding
  EXPECT_EQ(grip_value_for_surface(kCarSurfacePaved, spec), 256);
  EXPECT_EQ(grip_value_for_surface(kCarSurfaceDirt, spec), 192);
  EXPECT_EQ(grip_value_for_surface(kCarSurfaceIce, spec), 64);
  EXPECT_EQ(grip_value_for_surface(kCarSurfaceGrass, spec), 128);
  EXPECT_EQ(grip_value_for_surface(kCarSurfaceWater, spec), 0);

  // A non-zero water word (some future car) must be reported verbatim.
  spec.sliding_water = 300;
  EXPECT_EQ(grip_value_for_surface(kCarSurfaceWater, spec), 300);
}

// ---- Banked-loop front-wheel correction (seg001.asm loc_18228) -------------
// While the car is rolled past a deadband on a banked-corner element (codes
// 0x34..0x37 = 52..55) the front wheel response gains an extra roll / 5.

namespace {

// A stopped car on paved ground: its only front-wheel response is the banked
// term, so the tests can read the value straight back.
CarState grip_car(int16_t steering, uint16_t speed) {
  CarState car;
  car.car_sumSurfAllWheels = kCarWheelCount;
  car.car_steeringAngle = steering;
  car.car_actual_speed = speed;
  car.car_rev_speed = speed;
  for (int i = 0; i < kCarWheelCount; ++i) car.car_surfaceWhl[i] = kCarSurfacePaved;
  return car;
}

// The original reads the high byte of each 1/64-unit position accumulator, so
// the cell is the position divided by 65536 (one tile).
void place_car(CarState* car, int cell_x, int cell_z) {
  car->car_position.x = static_cast<int32_t>(cell_x) * 65536 + 32768;
  car->car_position.z = static_cast<int32_t>(cell_z) * 65536 + 32768;
}

Track track_with(int cell_x, int cell_z, uint8_t item) {
  Track track;
  track.items[static_cast<size_t>(cell_z) * Track::kSize + cell_x] = item;
  return track;
}

int16_t banked_response(int16_t roll, uint8_t item) {
  CarSpec spec = cornering_spec();
  CarState car = grip_car(0, 0);
  car.car_rotate.z = roll;
  place_car(&car, 5, 7);
  Track track = track_with(5, 7, item);
  update_grip(&car, spec, &track);
  return car.car_front_wheel_response_angle;
}

}  // namespace

TEST(banked_loop_tiles_add_roll_share_to_front_wheels) {
  // loc_182AE: 52..55 add car_rotate.z / 5 (idiv) to the front wheel angle.
  EXPECT_EQ(banked_response(50, 52), 10);
  EXPECT_EQ(banked_response(50, 53), 10);
  EXPECT_EQ(banked_response(50, 54), 10);
  EXPECT_EQ(banked_response(50, 55), 10);
}

TEST(banked_roll_share_is_signed_and_truncates_toward_zero) {
  EXPECT_EQ(banked_response(-50, 52), -10);
  EXPECT_EQ(banked_response(52, 52), 10);   // 52 / 5 truncates
  EXPECT_EQ(banked_response(-52, 52), -10);
  EXPECT_EQ(banked_response(55, 52), 11);
}

TEST(banked_share_needs_roll_above_the_deadband) {
  // loc_18241: |car_rotate.z| <= 4 is ignored.
  EXPECT_EQ(banked_response(0, 52), 0);
  EXPECT_EQ(banked_response(4, 52), 0);
  EXPECT_EQ(banked_response(-4, 52), 0);
  EXPECT_EQ(banked_response(5, 52), 1);
  EXPECT_EQ(banked_response(-5, 52), -1);
}

TEST(banked_share_only_on_banked_corner_elements) {
  // The elements just outside 0x34..0x37 (and plain road) must not trigger it.
  EXPECT_EQ(banked_response(50, 0x33), 0);  // 51
  EXPECT_EQ(banked_response(50, 0x38), 0);  // 56
  EXPECT_EQ(banked_response(50, 1), 0);     // plain road
  EXPECT_EQ(banked_response(50, 0), 0);     // empty cell
}

TEST(banked_share_needs_a_track) {
  CarSpec spec = cornering_spec();
  CarState car = grip_car(0, 0);
  car.car_rotate.z = 50;
  place_car(&car, 5, 7);
  update_grip(&car, spec, nullptr);
  EXPECT_EQ(car.car_front_wheel_response_angle, 0);
}

TEST(banked_share_resolves_continuation_cells) {
  // The car's cell may be a filler byte pointing at the multi-tile owner.
  CarSpec spec = cornering_spec();
  constexpr uint8_t kBankedItem = 52;

  // 0xFD -> owner one column left, one row down.
  {
    CarState car = grip_car(0, 0);
    car.car_rotate.z = 50;
    place_car(&car, 5, 7);
    Track track = track_with(5, 7, 0xFD);
    track.items[static_cast<size_t>(8) * Track::kSize + 4] = kBankedItem;
    update_grip(&car, spec, &track);
    EXPECT_EQ(car.car_front_wheel_response_angle, 10);
  }
  // 0xFE -> owner one row down.
  {
    CarState car = grip_car(0, 0);
    car.car_rotate.z = 50;
    place_car(&car, 5, 7);
    Track track = track_with(5, 7, 0xFE);
    track.items[static_cast<size_t>(8) * Track::kSize + 5] = kBankedItem;
    update_grip(&car, spec, &track);
    EXPECT_EQ(car.car_front_wheel_response_angle, 10);
  }
  // 0xFF -> owner one column left.
  {
    CarState car = grip_car(0, 0);
    car.car_rotate.z = 50;
    place_car(&car, 5, 7);
    Track track = track_with(5, 7, 0xFF);
    track.items[static_cast<size_t>(7) * Track::kSize + 4] = kBankedItem;
    update_grip(&car, spec, &track);
    EXPECT_EQ(car.car_front_wheel_response_angle, 10);
  }
}

TEST(banked_share_stacks_on_the_normal_front_wheel_response) {
  CarSpec spec = cornering_spec();

  CarState base = grip_car(55, 25600);
  place_car(&base, 5, 7);
  update_grip(&base, spec, nullptr);

  CarState banked = grip_car(55, 25600);
  banked.car_rotate.z = 30;
  place_car(&banked, 5, 7);
  Track track = track_with(5, 7, 52);
  update_grip(&banked, spec, &track);

  EXPECT_EQ(banked.car_front_wheel_response_angle - base.car_front_wheel_response_angle, 6);
}