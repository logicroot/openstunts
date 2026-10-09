#include <string>

#include "assets/car.h"
#include "core/paths.h"
#include "physics/car_engine.h"
#include "physics/car_state.h"
#include "tiny_test.h"

using namespace s3d;
using namespace s3d::physics;

namespace {

// A car resting on the ground, in gear 1, ready to be driven.
CarState make_ready_car(const CarSpec& spec) {
  CarState car;
  car.car_transmission = kTransmissionAutomatic;
  car.car_current_gear = 1;
  car.car_changing_gear = kGearChangeInactive;
  car.car_gearratio = spec.gear_ratios[1];
  car.car_gearratioshr8 = static_cast<uint16_t>(car.car_gearratio >> 8);
  car.car_currpm = spec.idle_rpm;
  car.car_sumSurfRearWheels = 1;
  car.car_sumSurfAllWheels = 1;
  return car;
}

}  // namespace

TEST(car_aero_resistance_table_matches_formula) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  CarSpec spec;
  if (!CarSpec::load(path_join(dir, "CARCOUN.RES"), &spec)) {
    std::printf("  (skipped: CARCOUN.RES not found)\n");
    return;
  }
  EXPECT_EQ(spec.aero_resistance_table[0], 0);
  for (int i = 0; i < 64; ++i) {
    int32_t prod = static_cast<int32_t>(spec.aero_resistance) * i;
    prod *= i;
    const int16_t expected = static_cast<int16_t>(static_cast<uint16_t>(prod >> 9));
    EXPECT_EQ(spec.aero_resistance_table[i], expected);
  }
}

TEST(car_engine_accelerates_and_upshifts) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  CarSpec spec;
  if (!CarSpec::load(path_join(dir, "CARCOUN.RES"), &spec)) return;

  CarState car = make_ready_car(spec);
  uint16_t top_speed = 0;
  uint16_t previous = 0;
  int upshifts = 0;
  int previous_gear = car.car_current_gear;

  // Ten seconds of full throttle at 20 Hz. Speed must never fall while the
  // clutch is engaged; during a gear change the original cuts drive torque
  // (statecar.c `accelerate_car`) so a tiny aero-drag dip is expected.
  for (int tick = 0; tick < 200; ++tick) {
    update_car_speed(kInputAccelerate, spec, &car, kGameFrameRateNormal, false, 200,
                     &top_speed);
    if (car.car_changing_gear == kGearChangeInactive) {
      EXPECT_TRUE(car.car_actual_speed >= previous);
    }
    previous = car.car_actual_speed;
    if (car.car_current_gear > previous_gear) ++upshifts;
    previous_gear = car.car_current_gear;
  }

  EXPECT_TRUE(car.car_actual_speed > 0);
  EXPECT_TRUE(top_speed > 0);
  EXPECT_TRUE(top_speed >= car.car_actual_speed);
  EXPECT_TRUE(upshifts >= 1);
  EXPECT_TRUE(car.car_is_accelerating == kPedalPressed);
  EXPECT_TRUE(car.car_currpm > spec.idle_rpm);
}

TEST(car_engine_braking_slows_the_car) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  CarSpec spec;
  if (!CarSpec::load(path_join(dir, "CARCOUN.RES"), &spec)) return;

  CarState car = make_ready_car(spec);
  for (int tick = 0; tick < 100; ++tick) {
    update_car_speed(kInputAccelerate, spec, &car, kGameFrameRateNormal);
  }
  const uint16_t fast = car.car_actual_speed;
  EXPECT_TRUE(fast > 0);

  for (int tick = 0; tick < 5; ++tick) {
    update_car_speed(kInputBrake, spec, &car, kGameFrameRateNormal);
    EXPECT_TRUE(car.car_actual_speed <= fast);
  }
  EXPECT_TRUE(car.car_actual_speed < fast);
  EXPECT_TRUE(car.car_is_braking == kPedalPressed);
}

TEST(car_engine_coasting_loses_speed) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  CarSpec spec;
  if (!CarSpec::load(path_join(dir, "CARCOUN.RES"), &spec)) return;

  CarState car = make_ready_car(spec);
  for (int tick = 0; tick < 100; ++tick) {
    update_car_speed(kInputAccelerate, spec, &car, kGameFrameRateNormal);
  }
  const uint16_t fast = car.car_actual_speed;
  for (int tick = 0; tick < 10; ++tick) {
    update_car_speed(kInputNone, spec, &car, kGameFrameRateNormal);
  }
  EXPECT_TRUE(car.car_actual_speed < fast);  // aero drag + engine braking
}
