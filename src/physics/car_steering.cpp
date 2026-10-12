#include "physics/car_steering.h"

#include <algorithm>

#include "physics/wrap_math.h"

namespace s3d::physics {

namespace {

// ---- Steering response tables (restunts headless_data.c) -------------------
// Four entries per speed row: {none, right, left, both}.
constexpr int8_t kSteerRespTable20fps[64] = {
    0, 8, -8, 0, 0, 7, -7, 0, 0, 6, -6, 0, 0, 5, -5, 0,
    0, 4, -4, 0, 0, 4, -4, 0, 0, 3, -3, 0, 0, 3, -3, 0,
    0, 2, -2, 0, 0, 2, -2, 0, 0, 2, -2, 0, 0, 1, -1, 0,
    0, 1, -1, 0, 0, 1, -1, 0, 0, 1, -1, 0, 0, 1, -1, 0};
constexpr int8_t kSteerRespTable10fps[62] = {
    0, 16, -16, 0, 0, 14, -14, 0, 0, 12, -12, 0, 0, 10, -10, 0,
    0, 8, -8, 0, 0, 8, -8, 0, 0, 6, -6, 0, 0, 6, -6, 0,
    0, 4, -4, 0, 0, 4, -4, 0, 0, 4, -4, 0, 0, 2, -2, 0,
    0, 2, -2, 0, 0, 1, -1, 0, 0, 1, -1, 0, 0, 1};

constexpr int kSteeringResponseSpeedShift = 10;
constexpr uint16_t kSteeringResponseIndexMask = 252;
constexpr int kSteeringLowRateResponseLimit = 160;
constexpr int kSteeringNormalResponseLimit = 80;
constexpr int16_t kSteeringAngleLimit = 240;
constexpr int16_t kSteeringCenterDeadzone = 8;
constexpr int16_t kSteeringAwayFromCenterThreshold = 1;
constexpr int kSteeringAwayResponseShift = 2;
constexpr int kSteeringCenteringSampleOffset = 1;
constexpr int kSteeringCenteringLimitShift = 1;

// ---- Grip constants (restunts physics_grip.c) ------------------------------
constexpr int kCarSpeedIntegerShift = 8;
constexpr int32_t kGripFixedScale = 256;
constexpr int kDemandedGripAngleShift = 3;
constexpr int kDemandedGripSpeedSquareShift = 6;
constexpr int kCombinedGripProductShift = 10;
constexpr int kFrontWheelAngleShift = 2;
constexpr int16_t kRotationRecenteringThreshold = 8;
constexpr int16_t kRotationRecenteringStep = 1;
constexpr int kSlideAngleWeight = 3;
constexpr int kSlideAngleBlendShift = 2;
constexpr int kSlideAngleDecayShift = 4;
constexpr int kSlideAngleFinalDecayShift = 1;
constexpr int16_t kSlideAngleDecayThreshold = 16;
constexpr int kSlideCorrectionDivisor = 14;
constexpr uint16_t kSlideGripTolerance = 1000;
constexpr int kSlideYawDampingDivisor = 2;
constexpr int kSlideSpeedPenaltyShift = 1;

// Speed penalty per grass wheel count: {0, 1, 2, 3, 4} wheels.
constexpr uint16_t kGrassDecelDivTab[5] = {255, 256, 192, 128, 64};

}  // namespace

// The per-surface grip values live in one array in the original struct:
// sliding_values[0] = simd->sliding (no contact), [1..4] = surfacegrip and
// [5] = the word at offset 190 (sliding_water), which the engine reads when a
// wheel is on water.
int16_t grip_value_for_surface(int surface, const CarSpec& spec) {
  switch (surface) {
    case 0:
      return spec.sliding;
    case kCarSurfacePaved:
      return spec.surface_grip[0];
    case kCarSurfaceDirt:
      return spec.surface_grip[1];
    case kCarSurfaceIce:
      return spec.surface_grip[2];
    case kCarSurfaceGrass:
      return spec.surface_grip[3];
    default:  // water (5) and any unknown id read the following struct word
      return spec.sliding_water;
  }
}

namespace {

// Grip demanded by the current steering angle + forward speed.
uint16_t grip_demanded(int16_t initial_angle, uint16_t speed_shr8) {
  const int16_t absolute_angle = absolute16(initial_angle);
  const int16_t angle_factor = sar16(absolute_angle, kDemandedGripAngleShift);
  uint16_t square_low = static_cast<uint16_t>(speed_shr8 * speed_shr8);
  square_low =
      static_cast<uint16_t>(square_low >> kDemandedGripSpeedSquareShift);
  return static_cast<uint16_t>(square_low * static_cast<uint16_t>(angle_factor));
}

// Combined grip = (grip * 2) * sum(sliding_values[surface]) >> 10.
int16_t grip_surface_sum(const CarState* car, const CarSpec& spec) {
  const int16_t combined_grip = shl16(spec.grip, 1);
  int16_t sliding_sum = 0;
  for (int i = 0; i < kCarWheelCount; ++i) {
    sliding_sum = wrap_add(sliding_sum, grip_value_for_surface(car->car_surfaceWhl[i], spec));
  }
  const uint32_t product = static_cast<uint32_t>(static_cast<int32_t>(combined_grip)) *
                           static_cast<uint32_t>(static_cast<int32_t>(sliding_sum));
  return static_cast<int16_t>(
      static_cast<uint16_t>(product >> kCombinedGripProductShift));
}

// Grass drag: speed -= speed / grassDecelDivTab[grass_count].
void grip_grass_drag(CarState* car) {
  int grass_wheels = 0;
  for (int i = 0; i < kCarWheelCount; ++i) {
    if (car->car_surfaceWhl[i] == kCarSurfaceGrass) ++grass_wheels;
  }
  if (grass_wheels != 0) {
    const uint16_t decel = static_cast<uint16_t>(
        car->car_actual_speed / kGrassDecelDivTab[grass_wheels]);
    car->car_actual_speed = static_cast<uint16_t>(car->car_actual_speed - decel);
    car->car_rev_speed = car->car_actual_speed;
  }
}

// Tiny straight-ahead yaw snap while the steering is fully centered.
void grip_recenter(CarState* car) {
  if (car->car_steeringAngle != kCarSteeringCentered) return;
  const int8_t rotation_low = static_cast<int8_t>(static_cast<uint8_t>(car->car_rotate.x));
  if (rotation_low == 0) return;
  const int16_t absolute_angle = rotation_low < 0 ? static_cast<int16_t>(-rotation_low)
                                                  : static_cast<int16_t>(rotation_low);
  if (absolute_angle >= kRotationRecenteringThreshold) return;
  car->car_rotate.x = rotation_low > 0 ? wrap_sub(car->car_rotate.x, kRotationRecenteringStep)
                                       : wrap_add(car->car_rotate.x, kRotationRecenteringStep);
}

// Slip angle: the steering angle the grip can sustain at this speed/wheels.
int16_t grip_slip_angle(CarState* car, int16_t initial_angle, int16_t combined_grip,
                        uint16_t demanded_grip, uint16_t speed_shr8) {
  int16_t adjusted_angle = initial_angle;
  if (static_cast<int16_t>(demanded_grip) > combined_grip) {
    car->car_slidingFlag = kCarSlidingActive;
    const int32_t numerator =
        static_cast<int32_t>(combined_grip) * kGripFixedScale;
    const int32_t denominator = static_cast<int32_t>(speed_shr8) * static_cast<int32_t>(speed_shr8);
    adjusted_angle = denominator == 0
                         ? 0
                         : static_cast<int16_t>(static_cast<uint16_t>(numerator / denominator));
    if (initial_angle < 0) adjusted_angle = wrap_negate(adjusted_angle);
    adjusted_angle = scale_cornering_angle(
        wrap_add(wrap_mul(adjusted_angle, kSlideAngleWeight), initial_angle), kSlideAngleBlendShift);
    car->car_slip_angle = wrap_sub(initial_angle, adjusted_angle);
  } else {
    car->car_slidingFlag = kCarSlidingInactive;
    if (car->car_slip_angle != 0) {
      car->car_slip_angle =
          wrap_sub(car->car_slip_angle, sar16(car->car_slip_angle, kSlideAngleDecayShift));
      const int16_t absolute_angle = absolute16(car->car_slip_angle);
      if (absolute_angle < kSlideAngleDecayThreshold) {
        car->car_slip_angle = sar16(car->car_slip_angle, kSlideAngleFinalDecayShift);
      }
    }
  }
  return adjusted_angle;
}

// Slide-yaw accumulation with grip tolerance and one-half damping per tick.
void grip_player_yaw(CarState* car, int16_t initial_angle, int16_t adjusted_angle,
                     int16_t combined_grip, uint16_t demanded_grip) {
  const int16_t correction = div_or_zero(wrap_sub(adjusted_angle, initial_angle),
                                         static_cast<int16_t>(kSlideCorrectionDivisor));
  const int16_t tolerated_grip =
      static_cast<int16_t>(static_cast<uint16_t>(
          static_cast<uint16_t>(combined_grip) + kSlideGripTolerance));
  if (tolerated_grip < static_cast<int16_t>(demanded_grip)) {
    car->car_slide_yaw_delta =
        wrap_add(car->car_slide_yaw_delta, correction);
    car->car_slide_yaw_delta =
        div_or_zero(car->car_slide_yaw_delta, static_cast<int16_t>(kSlideYawDampingDivisor));
  } else if (car->car_slide_yaw_delta != 0) {
    car->car_slide_yaw_delta =
        wrap_add(car->car_slide_yaw_delta, correction);
    car->car_slide_yaw_delta =
        div_or_zero(car->car_slide_yaw_delta, static_cast<int16_t>(kSlideYawDampingDivisor));
    if (car->car_slide_yaw_delta == 0) {
      car->car_actual_speed = static_cast<uint16_t>(fixed::multiply_and_scale(
          fixed::cos_fast(static_cast<uint16_t>(car->car_velocity_heading_offset)),
          static_cast<int16_t>(car->car_actual_speed)));
      if (fixed::cos_fast(static_cast<uint16_t>(car->car_velocity_heading_offset)) < 0) {
        car->car_actual_speed = kSpeedStopped;
      }
      car->car_velocity_heading_offset = 0;
    }
  }
}

// Velocity heading after slides/rotation damping.
void grip_heading_offset(CarState* car) {
  if (car->car_velocity_heading_offset != 0 && car->car_slide_yaw_delta == 0) {
    car->car_velocity_heading_offset = damp_collision_angle(car->car_velocity_heading_offset);
  }
  if (car->car_slide_yaw_delta != 0) {
    car->car_velocity_heading_offset =
        wrap_sub(car->car_velocity_heading_offset, car->car_slide_yaw_delta);
  }
}

// Lateral speed drained by sliding, plus tyre sound flags.
void grip_sliding_speed(CarState* car) {
  if (car->car_slidingFlag == kCarSlidingInactive) return;
  const int16_t penalty = shl16(absolute16(car->car_slip_angle), kSlideSpeedPenaltyShift);
  const uint16_t unsigned_penalty = static_cast<uint16_t>(penalty);
  if (car->car_rev_speed <= unsigned_penalty) {
    car->car_rev_speed = kSpeedStopped;
    car->car_actual_speed = kSpeedStopped;
  } else if (car->car_actual_speed > unsigned_penalty) {
    car->car_rev_speed = static_cast<uint16_t>(car->car_rev_speed - unsigned_penalty);
    car->car_actual_speed = static_cast<uint16_t>(car->car_actual_speed - unsigned_penalty);
  } else {
    car->car_rev_speed = kSpeedStopped;
    car->car_actual_speed = kSpeedStopped;
  }

  if (car->car_crashBmpFlag == kCrashEventNone) {
    int i = 0;
    for (; i < kCarWheelCount; ++i) {
      if (car->car_surfaceWhl[i] == kCarSurfacePaved) break;
    }
    car->car_sound_flags = static_cast<int8_t>(
        car->car_sound_flags |
        (i < kCarWheelCount ? kCarSoundSkidPaved : kCarSoundSkidOffroad));
  }
}

// Player grip response: recenter, adjust slip, then banked + yaw corrections.
// Banked-loop steering (tiles 52..55) needs the track element map; deferred.
void grip_player_response(CarState* car, int16_t initial_angle, int16_t combined_grip,
                          uint16_t demanded_grip, uint16_t speed_shr8) {
  grip_recenter(car);
  const int16_t adjusted_angle =
      grip_slip_angle(car, initial_angle, combined_grip, demanded_grip, speed_shr8);
  if (car->car_slide_yaw_delta == 0 && car->car_crashBmpFlag != kCrashEventCollision) {
    car->car_front_wheel_response_angle = adjusted_angle;
  } else {
    car->car_front_wheel_response_angle = 0;
  }
  grip_player_yaw(car, initial_angle, adjusted_angle, combined_grip, demanded_grip);
}

}  // namespace

int16_t scale_cornering_angle(int16_t angle, int shift) {
  return sar16(angle, shift);  // left-corner bias enabled (the default)
}

int16_t damp_collision_angle(int16_t angle) {
  return sar16(wrap_mul(angle, 15), 4);
}

void update_player_steering(uint8_t input_flags, CarState* car, int frames_per_sec) {
  const bool low_rate = frames_per_sec == kGameFrameRateLow;
  const int16_t steering_input =
      static_cast<int16_t>(sar16(static_cast<int16_t>(input_flags), 2) &
                           static_cast<int16_t>(kInputPedalMask));

  int16_t steering_angle = car->car_steeringAngle;
  const int16_t speed_index = static_cast<int16_t>(
      (static_cast<uint16_t>(car->car_actual_speed) >> kSteeringResponseSpeedShift) &
      kSteeringResponseIndexMask);
  const int16_t response_index = wrap_add(speed_index, steering_input);

  // The 10 Hz table has 62 entries; indices 62/63 (top speed) read 0 here.
  int16_t response = (response_index < (low_rate ? 62 : 64))
                         ? (low_rate ? kSteerRespTable10fps[response_index]
                                     : kSteerRespTable20fps[response_index])
                         : 0;

  // Turning farther from center gets the original fourfold response.
  if ((response > 0 && steering_angle < -kSteeringAwayFromCenterThreshold) ||
      (response < 0 && steering_angle > kSteeringAwayFromCenterThreshold)) {
    response = shl16(response, kSteeringAwayResponseShift);
  }

  // With no steering input, bring a moving car back toward center.
  if (response == 0 && car->car_actual_speed != kSpeedStopped &&
      steering_angle != kCarSteeringCentered) {
    // speed_index + 1 is the centre-of-range column of the next row.
    const int16_t sample_index = response_index + 1;
    const int16_t center_sample =
        (sample_index < (low_rate ? 62 : 64))
            ? (low_rate ? kSteerRespTable10fps[sample_index]
                        : kSteerRespTable20fps[sample_index])
            : 0;
    const int16_t centering_limit = shl16(center_sample, kSteeringCenteringLimitShift);
    if (steering_angle < 0) {
      response = (wrap_negate(steering_angle) > centering_limit)
                     ? centering_limit
                     : wrap_negate(steering_angle);
    } else {
      response = (steering_angle > centering_limit) ? wrap_negate(centering_limit)
                                                    : wrap_negate(steering_angle);
    }
  }

  // Clamp the response to the tick rate limit.
  const int16_t response_limit = low_rate ? kSteeringLowRateResponseLimit
                                          : kSteeringNormalResponseLimit;
  response = std::clamp(response, static_cast<int16_t>(-response_limit),
                        static_cast<int16_t>(response_limit));

  steering_angle = wrap_add(steering_angle, response);
  steering_angle = std::clamp< int16_t>(steering_angle,
      static_cast<int16_t>(-kSteeringAngleLimit),
      static_cast<int16_t>(kSteeringAngleLimit));

  const int16_t center_value = (response_index & 63) < (low_rate ? 62 : 64)
                                   ? (low_rate ? kSteerRespTable10fps[response_index & 63]
                                               : kSteerRespTable20fps[response_index & 63])
                                   : 0;
  if (center_value == 0 && steering_angle > -kSteeringCenterDeadzone &&
      steering_angle < kSteeringCenterDeadzone) {
    steering_angle = kCarSteeringCentered;
  }

  car->car_steeringAngle = steering_angle;
}

void update_grip(CarState* car, const CarSpec& spec) {
  if (car->car_sumSurfAllWheels == kWheelContactNone) {
    car->car_front_wheel_response_angle = 0;
    car->car_slidingFlag = kCarSlidingInactive;
    return;
  }

  grip_grass_drag(car);
  const int16_t initial_angle =
      wrap_add(car->car_steeringAngle, car->car_velocity_heading_offset);
  const uint16_t speed_shr8 = static_cast<uint16_t>(car->car_rev_speed >> kCarSpeedIntegerShift);
  const uint16_t demanded_grip = grip_demanded(initial_angle, speed_shr8);
  const int16_t combined_grip = grip_surface_sum(car, spec);
  car->car_demandedGrip = static_cast<int16_t>(demanded_grip);
  car->car_surfacegrip_sum = combined_grip;

  // Player behaviour only; opponent steering updates are not reachable yet.
  grip_player_response(car, initial_angle, combined_grip, demanded_grip, speed_shr8);

  grip_heading_offset(car);
  grip_sliding_speed(car);
  car->car_slip_angle = 0;
}

}  // namespace s3d::physics