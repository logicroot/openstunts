#include "physics/car_engine.h"

namespace s3d::physics {

namespace {

// 16-bit wrapping helpers matching the original DOS `int` arithmetic.
int16_t add16(int a, int b) { return static_cast<int16_t>(static_cast<uint16_t>(a + b)); }
int16_t sub16(int a, int b) { return static_cast<int16_t>(static_cast<uint16_t>(a - b)); }
int16_t neg16(int a) { return static_cast<int16_t>(static_cast<uint16_t>(-a)); }

int abs_word(int a) {
  const int16_t s = static_cast<int16_t>(a);
  return s < 0 ? -s : s;
}

// statecar.c constants.
constexpr int kAccelerationMassNumerator = 25;
constexpr int kAccelerationDragScale = 200;
constexpr int kNormalGearKnobStep = 6;
constexpr int kLowRateGearKnobStep = 12;
constexpr unsigned kGearRatioByteShift = 8;
constexpr unsigned kAerodynamicResistanceSpeedShift = 10;
constexpr int kLowRateGearChangeRpmDrop = 80;
constexpr int kNormalGearChangeRpmDrop = 40;
constexpr uint16_t kAirborneMaxSpeed = 64000;
constexpr int kAirborneAcceleration = 768;
constexpr int kIdleTorqueRpmThreshold = 2600;
constexpr unsigned kTorqueCurveRpmShift = 7;
constexpr int kEngineLimiterBlendRpm = 5000;
constexpr unsigned kTorqueAccelerationShift = 4;
constexpr int kOpponentSpeedScale = 200;
constexpr int kOpponentDragShift = 1;
constexpr int kEngineLimiterAccelerationThreshold = 296;
constexpr int kEngineLimiterShortTicks = 5;
constexpr int kOpponentBrakingMultiplier = 2;
constexpr uint16_t kWrappedReverseSpeedLimit = 62720;
constexpr int kWheelSpeedSyncThreshold = 5120;
constexpr int kRapidRpmChangeThreshold = 2000;
constexpr int kEngineTorqueLimitThreshold = 12000;
constexpr int kEngineLimiterLongTicks = 30;
constexpr int kEngineLimiterRecoveryTicks = 10;
constexpr uint16_t kEngineSpeedCorrection = 1280;
constexpr unsigned kAccelerationMassResultShift = 1;
constexpr unsigned kEngineLimiterTorqueBlendShift = 1;
constexpr unsigned kWheelSpeedAverageShift = 1;
constexpr int kMaxRpmMargin = 1;
constexpr int kEngineLimiterInactive = 0;
constexpr int kEngineLimiterTickStep = 1;
constexpr int kGearNeutral = 0;
constexpr int kGearFirst = 1;
constexpr int kGearIndexStep = 1;
constexpr int kGearChangeDelayHalfShift = 1;
constexpr int kGearChangeDelayTickStep = 1;
constexpr int kGearKnobAligned = 0;
constexpr int kGearKnobDirectionNeutral = 0;
constexpr int kCarSpeedDeltaStationary = 0;
constexpr uint16_t kLegacySignBit = 0x8000;

enum { kShiftDown = -1, kShiftNone = 0, kShiftUp = 1 };

int16_t scale_acceleration_by_mass(int16_t acceleration, int16_t mass) {
  const int32_t product =
      static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(acceleration) *
                                                kAccelerationMassNumerator));
  // Powergear bug (enabled by default): the original uses an unsigned divide,
  // so a negative force wraps to a large positive dividend.
  int32_t quotient;
  if (mass == 0) {
    quotient = 0;
  } else {
    quotient = static_cast<int32_t>(static_cast<uint32_t>(product) /
                                    static_cast<uint16_t>(mass));
  }
  const int16_t low_word = static_cast<int16_t>(static_cast<uint16_t>(quotient));
  return add16(low_word >> kAccelerationMassResultShift, 0);
}

int16_t apply_opponent_acceleration_drag(int16_t acceleration, uint8_t drag) {
  const int32_t product = static_cast<int32_t>(static_cast<uint32_t>(
      static_cast<uint16_t>(drag) * static_cast<int32_t>(acceleration)));
  const int32_t reduction = product / kAccelerationDragScale;
  return sub16(acceleration, static_cast<int16_t>(static_cast<uint16_t>(reduction)));
}

int8_t gear_change_delay(int frame_rate) {
  const int16_t signed_rate = static_cast<int8_t>(static_cast<uint8_t>(frame_rate));
  const int16_t half_rate = static_cast<int16_t>(signed_rate >> kGearChangeDelayHalfShift);
  return static_cast<int8_t>(static_cast<uint8_t>(
      static_cast<uint8_t>(half_rate) + static_cast<uint8_t>(frame_rate)));
}

int16_t move_gear_knob_toward(int16_t current, int16_t target, int16_t step) {
  const int16_t difference = sub16(target, current);
  if (abs_word(difference) <= step) return target;
  if (difference > kGearKnobDirectionNeutral) return add16(current, step);
  return sub16(current, step);
}

uint16_t update_rpm_from_speed(uint16_t currpm, uint16_t speed, uint16_t gearratio,
                               int16_t changing_gear, uint16_t idle_rpm) {
  if (changing_gear == kGearChangeInactive) {
    currpm = static_cast<uint16_t>(
        (static_cast<uint32_t>(speed) * static_cast<uint16_t>(gearratio)) >> 16);
  }
  return currpm >= idle_rpm ? currpm : idle_rpm;
}

int16_t gear_shift_direction(uint8_t input_flags, const CarState& car, const CarSpec& spec) {
  if (car.car_transmission == kTransmissionManual &&
      car.car_changing_gear == kGearChangeInactive) {
    if ((input_flags & kInputPrimaryAction) != 0) return kShiftUp;
    if ((input_flags & kInputSecondaryAction) != 0) return kShiftDown;
  } else if (car.car_current_gear != kGearNeutral &&
             car.car_changing_gear == kGearChangeInactive &&
             car.car_sumSurfRearWheels != kWheelContactNone) {
    if (static_cast<uint16_t>(car.car_currpm) >
        static_cast<uint16_t>(spec.upshift_rpm)) {
      return kShiftUp;
    }
    if (static_cast<uint16_t>(car.car_currpm) <
        static_cast<uint16_t>(spec.downshift_rpm)) {
      return kShiftDown;
    }
  }
  return kShiftNone;
}

void request_gear_change(uint8_t input_flags, CarState* car, const CarSpec& spec,
                         int frames_per_sec) {
  int16_t shift_direction = gear_shift_direction(input_flags, *car, spec);
  if (shift_direction == kShiftUp && car->car_current_gear != spec.num_gears) {
    car->car_current_gear =
        static_cast<int8_t>(static_cast<uint8_t>(car->car_current_gear) + kGearIndexStep);
  } else if (shift_direction == kShiftDown && car->car_current_gear > kGearFirst) {
    car->car_current_gear =
        static_cast<int8_t>(static_cast<uint8_t>(car->car_current_gear) - kGearIndexStep);
  } else {
    shift_direction = kShiftNone;
  }
  if (shift_direction != kShiftNone) {
    car->car_changing_gear = kGearChangeActive;
    car->car_gear_change_delay = gear_change_delay(frames_per_sec);
    car->car_knob_x2 = spec.knob_points[static_cast<uint8_t>(car->car_current_gear)].x;
    car->car_knob_y2 = spec.knob_points[static_cast<uint8_t>(car->car_current_gear)].y;
  }
}

void update_gear_knob(CarState* car, const CarSpec& spec, int frames_per_sec) {
  const int16_t step =
      frames_per_sec == kGameFrameRateNormal ? kNormalGearKnobStep : kLowRateGearKnobStep;
  if (car->car_changing_gear != kGearChangeInactive) {
    if (car->car_knob_x == car->car_knob_x2) {
      const int16_t knob_delta = sub16(car->car_knob_y2, car->car_knob_y);
      if (knob_delta == kGearKnobAligned) {
        car->car_changing_gear = kGearChangeInactive;
        car->car_gearratio = spec.gear_ratios[static_cast<uint8_t>(car->car_current_gear)];
        car->car_gearratioshr8 =
            static_cast<uint16_t>(car->car_gearratio >> kGearRatioByteShift);
      } else {
        car->car_knob_y = move_gear_knob_toward(car->car_knob_y, car->car_knob_y2, step);
      }
    } else if (spec.knob_points[kGearNeutral].y == car->car_knob_y) {
      car->car_knob_x = move_gear_knob_toward(car->car_knob_x, car->car_knob_x2, step);
    } else {
      car->car_knob_y =
          move_gear_knob_toward(car->car_knob_y, spec.knob_points[kGearNeutral].y, step);
    }
  } else if (car->car_gear_change_delay != kGearChangeDelayExpired) {
    car->car_gear_change_delay = static_cast<int8_t>(static_cast<uint8_t>(
        car->car_gear_change_delay - kGearChangeDelayTickStep));
  }
}

int16_t grounded_acceleration_delta(bool is_opponent, uint8_t opponent_target_speed,
                                    CarState* car, const CarSpec& spec, int16_t speed_delta) {
  uint8_t torque_or_drag;
  if (car->car_current_gear <= kGearFirst && car->car_currpm < kIdleTorqueRpmThreshold) {
    torque_or_drag = spec.idle_torque;
  } else {
    torque_or_drag =
        spec.torque_curve[static_cast<uint16_t>(car->car_currpm) >> kTorqueCurveRpmShift];
  }
  if (car->car_engineLimiterTimer != kEngineLimiterInactive &&
      car->car_currpm < kEngineLimiterBlendRpm) {
    torque_or_drag = static_cast<uint8_t>(
        (static_cast<uint8_t>(spec.idle_torque) + torque_or_drag) >>
        kEngineLimiterTorqueBlendShift);
  }
  speed_delta = add16(speed_delta,
                      static_cast<int16_t>(static_cast<uint16_t>(
                          static_cast<uint16_t>(
                              static_cast<uint16_t>(car->car_gearratioshr8) *
                              static_cast<uint16_t>(torque_or_drag)) >>
                          kTorqueAccelerationShift)));
  speed_delta = scale_acceleration_by_mass(speed_delta, spec.car_mass);
  if (is_opponent) {
    torque_or_drag =
        static_cast<uint8_t>((kOpponentSpeedScale - opponent_target_speed) >> kOpponentDragShift);
    if (torque_or_drag != 0) {
      speed_delta = apply_opponent_acceleration_drag(speed_delta, torque_or_drag);
    }
  }
  if (speed_delta > kEngineLimiterAccelerationThreshold) {
    car->car_engineLimiterTimer = kEngineLimiterShortTicks;
  }
  return speed_delta;
}

int16_t accelerate_car(bool is_opponent, uint8_t opponent_target_speed, CarState* car,
                       const CarSpec& spec, int16_t speed_delta, int frames_per_sec) {
  if (car->car_changing_gear != kGearChangeInactive) {
    car->car_engineLimiterTimer = kEngineLimiterInactive;
    if (frames_per_sec == kGameFrameRateLow) {
      car->car_currpm = sub16(car->car_currpm, kLowRateGearChangeRpmDrop);
    } else {
      car->car_currpm = sub16(car->car_currpm, kNormalGearChangeRpmDrop);
    }
  } else if (car->car_sumSurfRearWheels == kWheelContactNone) {
    if (static_cast<uint16_t>(car->car_currpm) < static_cast<uint16_t>(spec.max_rpm) &&
        car->car_rev_speed < kAirborneMaxSpeed) {
      speed_delta = add16(speed_delta, kAirborneAcceleration);
    }
  } else {
    speed_delta = grounded_acceleration_delta(is_opponent, opponent_target_speed, car, spec,
                                              speed_delta);
  }
  return speed_delta;
}

int16_t pedal_speed_delta(uint8_t input_flags, bool is_opponent,
                          uint8_t opponent_target_speed, CarState* car, const CarSpec& spec,
                          int frames_per_sec) {
  int16_t speed_delta = sub16(
      car->car_pseudoGravity,
      spec.aero_resistance_table[car->car_rev_speed >> kAerodynamicResistanceSpeedShift]);
  if (static_cast<uint16_t>(car->car_currpm) > static_cast<uint16_t>(spec.max_rpm)) {
    car->car_currpm = sub16(spec.max_rpm, kMaxRpmMargin);
    speed_delta = sub16(speed_delta, spec.braking_eff);
  } else if ((input_flags & kInputPedalMask) == kInputAccelerate) {
    car->car_is_braking = kPedalReleased;
    car->car_is_accelerating = kPedalPressed;
    speed_delta = accelerate_car(is_opponent, opponent_target_speed, car, spec, speed_delta,
                                 frames_per_sec);
  } else if ((input_flags & kInputPedalMask) == kInputBrake) {
    car->car_is_accelerating = kPedalReleased;
    car->car_engineLimiterTimer = kEngineLimiterInactive;
    car->car_is_braking = kPedalPressed;
    if (!is_opponent) {
      speed_delta = sub16(speed_delta, spec.braking_eff);
    } else {
      speed_delta =
          sub16(speed_delta, static_cast<int16_t>(spec.braking_eff * kOpponentBrakingMultiplier));
    }
  } else {
    car->car_is_accelerating = kPedalReleased;
    car->car_is_braking = kPedalReleased;
  }
  if (frames_per_sec == kGameFrameRateLow) {
    speed_delta = add16(speed_delta, speed_delta);
  }
  return speed_delta;
}

uint16_t apply_speed_delta(uint16_t updated_speed, int16_t speed_delta) {
  if (speed_delta >= kCarSpeedDeltaStationary) {
    if (updated_speed < kLegacySignBit) {
      updated_speed = static_cast<uint16_t>(updated_speed + static_cast<uint16_t>(speed_delta));
    } else {
      updated_speed = static_cast<uint16_t>(updated_speed + static_cast<uint16_t>(speed_delta));
      if (updated_speed < kLegacySignBit || updated_speed > kWrappedReverseSpeedLimit) {
        updated_speed = kWrappedReverseSpeedLimit;
      }
    }
  } else if (static_cast<uint16_t>(neg16(speed_delta)) > updated_speed) {
    updated_speed = kSpeedStopped;
  } else {
    updated_speed = static_cast<uint16_t>(updated_speed + static_cast<uint16_t>(speed_delta));
  }
  return updated_speed;
}

void synchronize_wheel_speed(CarState* car, uint16_t updated_speed) {
  if (car->car_sumSurfRearWheels == kWheelContactNone) {
    car->car_rev_speed = updated_speed;
  } else {
    const int16_t speed_difference =
        static_cast<int16_t>(abs_word(sub16(car->car_actual_speed, updated_speed)));
    if (speed_difference > kWheelSpeedSyncThreshold) {
      car->car_rev_speed = static_cast<uint16_t>(
          (static_cast<uint32_t>(car->car_rev_speed) + car->car_actual_speed) >>
          kWheelSpeedAverageShift);
      car->car_actual_speed = car->car_rev_speed;
      car->car_engineLimiterTimer = kEngineLimiterShortTicks;
    } else {
      car->car_rev_speed = updated_speed;
      car->car_actual_speed = updated_speed;
    }
  }
}

void update_engine_limiter(CarState* car, const CarSpec& spec) {
  if (car->car_sumSurfAllWheels != kWheelContactNone &&
      car->car_lastrpm > car->car_currpm) {
    if (sub16(car->car_lastrpm, car->car_currpm) > kRapidRpmChangeThreshold) {
      if (spec.idle_torque * car->car_gearratioshr8 > kEngineTorqueLimitThreshold) {
        car->car_engineLimiterTimer = kEngineLimiterLongTicks;
      }
    } else if (sub16(car->car_currpm, car->car_lastrpm) > kRapidRpmChangeThreshold) {
      car->car_engineLimiterTimer = kEngineLimiterRecoveryTicks;
      car->car_actual_speed =
          static_cast<uint16_t>(car->car_actual_speed - kEngineSpeedCorrection);
    }
  }
}

}  // namespace

void update_car_speed(uint8_t input_flags, const CarSpec& spec, CarState* car,
                      int frames_per_sec, bool is_opponent, uint8_t opponent_target_speed,
                      uint16_t* top_speed) {
  if (car->car_engineLimiterTimer != kEngineLimiterInactive) {
    car->car_engineLimiterTimer =
        static_cast<int8_t>(static_cast<uint8_t>(car->car_engineLimiterTimer -
                                                 kEngineLimiterTickStep));
  }
  car->car_speeddiff = sub16(car->car_actual_speed, car->car_lastspeed);
  car->car_lastspeed = car->car_actual_speed;
  car->car_lastrpm = car->car_currpm;

  request_gear_change(input_flags, car, spec, frames_per_sec);
  update_gear_knob(car, spec, frames_per_sec);
  const int16_t speed_delta = pedal_speed_delta(
      input_flags, is_opponent, opponent_target_speed, car, spec, frames_per_sec);
  const uint16_t updated_speed = apply_speed_delta(car->car_rev_speed, speed_delta);
  synchronize_wheel_speed(car, updated_speed);
  car->car_currpm = update_rpm_from_speed(car->car_currpm, car->car_rev_speed,
                                          car->car_gearratio, car->car_changing_gear,
                                          spec.idle_rpm);
  update_engine_limiter(car, spec);
  if (top_speed != nullptr && car->car_actual_speed > *top_speed) {
    *top_speed = car->car_actual_speed;
  }
}

}  // namespace s3d::physics
