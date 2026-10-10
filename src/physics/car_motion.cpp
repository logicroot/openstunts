#include "physics/car_motion.h"

#include <algorithm>

#include "physics/car_steering.h"
#include "physics/fixed_math.h"
#include "physics/wrap_math.h"
#include "physics/track_collision.h"

namespace s3d::physics {

namespace {

// ---- Pose/resolution constants (restunts stateply.c) -----------------------
constexpr int kFrontWheelHeadingShift = 2;
constexpr uint32_t kTravelNumerator = 1408;
constexpr uint32_t kTravelDivisorNormal = 15360;
constexpr uint32_t kTravelDivisorLow = 7680;
constexpr int16_t kSuspensionTravelLimit = 384;
constexpr int16_t kPseudoGravityLength = 130;
constexpr int16_t kContactDistanceLimit = 12;      // PHYSICS_PLANE_CONTACT_TOLERANCE
constexpr int16_t kRotationDeadband = 2;
constexpr int32_t kWorldMinPosition = 3840;
constexpr int32_t kWorldMaxPosition = 1962239;
constexpr int32_t kWorldMaxExclusive = 1962240;
constexpr int kWorldToLongShift = 6;
constexpr int16_t kGravityStep[4] = {21, 21, 15, 15};  // wheel_gravity_steps

// ---- Suspension constants (restunts physics_collision.c) -------------------
constexpr int16_t kSuspensionTargetDecay = 4;
constexpr int16_t kSuspensionReturnStep = 128;
constexpr int16_t kSuspensionContactDeltaLimit = 192;
constexpr int16_t kSuspensionSoftContactThreshold = -288;
constexpr int16_t kHardContactScaleNumerator = 3;

constexpr int kWheelCentroidShift = 2;

// Pairwise sum (a + b) - c - d with 16-bit wrapping (wheel_pair_delta).
int16_t wheel_pair_delta(int16_t first, int16_t second, int16_t third, int16_t fourth) {
  return wrap_sub(wrap_sub(first + second, third), fourth);
}

// Speed -> forward travel per tick: speed * 1408 / divisor (world units).
int16_t scale_speed_to_travel(uint16_t speed, uint32_t divisor) {
  const uint32_t product = static_cast<uint32_t>(speed) * kTravelNumerator;
  return divisor == 0 ? 0 : static_cast<int16_t>(static_cast<uint16_t>(product / divisor));
}

// Angle of the vector (z, y) in 0x400 units via the shared fixed math.
int16_t polar_angle16(int z, int y) { return static_cast<int16_t>(fixed::polar_angle(z, y)); }

int16_t apply_rotation_deadband(int16_t angle) {
  return absolute16(angle) < kRotationDeadband ? 0 : angle;
}

// Warps (current - previous) even at the 16-bit distance edges, keeping
// orientation stable when wheels straddle the world wraparound.
int16_t difference_word_wrap(int32_t left, int32_t right) {
  return static_cast<int16_t>(wrap_sub32(left, right));
}

}  // namespace

int16_t update_wheel_suspension(CarState* car, int16_t wheel_index, int16_t contact_delta) {
  const int16_t previous_deflection = car->car_suspension_deflection[wheel_index];

  // Decay the per-wheel target toward zero at the requested tick rate.
  int16_t target = car->car_suspension_target[wheel_index];
  if (target < 0) {
    target = wrap_add(target, kSuspensionTargetDecay);
    if (target > 0) target = 0;
  } else if (target > 0) {
    target = wrap_sub(target, kSuspensionTargetDecay);
    if (target < 0) target = 0;
  }
  car->car_suspension_target[wheel_index] = target;

  // A resting wheel worn into the suspension absorbs small positive deltas.
  if (contact_delta < 0 && car->car_suspension_deflection[wheel_index] > wrap_negate(contact_delta)) {
    contact_delta = 0;
  }

  int16_t adjustment = 0;
  int16_t deflection = car->car_suspension_deflection[wheel_index];
  if (contact_delta == 0) {
    // Return toward the (decayed) target when there is no contact change. The
    // original reports the actual per-wheel movement (previous - new) in the
    // compression branch and zero in the extension branch; reproduce both.
    int16_t adjustment_ret = 0;
    if (deflection > target) {
      deflection = wrap_sub(deflection, kSuspensionReturnStep);
      if (deflection < target) deflection = target;
      adjustment_ret = wrap_sub(previous_deflection, deflection);
    } else if (deflection < target) {
      deflection = wrap_add(deflection, kSuspensionReturnStep);
      if (deflection > target) deflection = target;
      adjustment_ret = 0;
    }
    car->car_suspension_deflection[wheel_index] = deflection;
    adjustment = adjustment_ret;
  } else if (contact_delta > 0) {
    // Wheel pushed into the body: compress the spring, clamp to travel limit.
    if (contact_delta > kSuspensionContactDeltaLimit) {
      contact_delta = kSuspensionContactDeltaLimit;
    }
    deflection = wrap_add(deflection, contact_delta);
    if (deflection > kSuspensionTravelLimit) deflection = kSuspensionTravelLimit;
    car->car_suspension_deflection[wheel_index] = deflection;
    car->car_reserved_contact_state[wheel_index] = 0;
  } else {
    // Wheel dropped below the contact: extend the spring.
    if (wrap_add(contact_delta, deflection) > kSuspensionSoftContactThreshold) {
      deflection = wrap_add(deflection, contact_delta);
    } else {
      const int16_t scaled_delta =
          sar16(wrap_mul(contact_delta, kHardContactScaleNumerator), 2);
      deflection = wrap_add(deflection, scaled_delta);
      if (deflection < -kSuspensionTravelLimit) deflection = -kSuspensionTravelLimit;
    }
    car->car_suspension_deflection[wheel_index] = deflection;
    adjustment = wrap_add(wrap_sub(previous_deflection, deflection), contact_delta);
  }
  return wrap_add(previous_deflection, adjustment);
}

void step_car_motion(CarState* car, const CarSpec& spec, int frames_per_sec,
                     const GroundSampler& ground, const TrackCollision* collision) {
  (void)collision;
  // ---- Working pose --------------------------------------------------------
  int32_t work_x = car->car_position.x;
  int32_t work_y = car->car_position.y;
  int32_t work_z = car->car_position.z;
  int16_t work_roll = car->car_rotate.z;   // original "car_rotate.z" = roll
  int16_t work_pitch = car->car_rotate.y;
  int16_t work_yaw = car->car_rotate.x;

  // ---- Travel and front-wheel heading --------------------------------------
  const uint32_t divisor = frames_per_sec == kGameFrameRateLow ? kTravelDivisorLow
                                                               : kTravelDivisorNormal;
  const int16_t travel = scale_speed_to_travel(car->car_actual_speed, divisor);
  const int16_t response_angle = car->car_front_wheel_response_angle;
  const int16_t front_heading_offset =
      (car->car_sumSurfAllWheels != kWheelContactNone && travel != 0)
          ? scale_cornering_angle(response_angle, kFrontWheelHeadingShift)
          : 0;

  // ---- Car-to-world rotation (roll/pitch/yaw negated, ZXY order) -----------
  const fixed::Mat3 car_to_world =
      fixed::mat_rot_zxy(wrap_negate(work_roll), wrap_negate(work_pitch), wrap_negate(work_yaw), 0);

  // Pseudo-gravity: how far "down" the world's gravity pulls in car space.
  if (work_pitch != 0 || work_roll != 0) {
    const fixed::Vec3 gravity{0, 0, kPseudoGravityLength};
    fixed::Vec3 world_down;
    fixed::mat_mul_vector(gravity, car_to_world, &world_down);
    car->car_pseudoGravity = wrap_negate(world_down.y);
  } else {
    car->car_pseudoGravity = 0;
  }

  // ---- Wheel placement and travel ------------------------------------------
  const int16_t slide_rotation = car->car_slide_yaw_delta;
  const bool has_slide = (static_cast<uint16_t>(slide_rotation) & 0x3FF) != 0;
  fixed::Mat3 slide_adjust_rotation;
  if (has_slide) {
    slide_adjust_rotation = fixed::mat_rot_zxy(0, 0, wrap_negate(slide_rotation), 0);
  }

  Vec3Long current[4];
  Vec3Long previous[4];
  for (int i = 0; i < kCarWheelCount; ++i) {
    fixed::Vec3 wheel_vector;
    wheel_vector.x = spec.wheel_coords[i].x;
    wheel_vector.y = wrap_negate(wrap_add(car->car_suspension_deflection[i], kSuspensionTravelLimit));
    wheel_vector.z = spec.wheel_coords[i].z;
    if (has_slide) {
      fixed::Vec3 rotated;
      fixed::mat_mul_vector(wheel_vector, slide_adjust_rotation, &rotated);
      wheel_vector = rotated;
    }
    fixed::Vec3 world_offset;
    fixed::mat_mul_vector(wheel_vector, car_to_world, &world_offset);
    current[i].x = wrap_add32(work_x, world_offset.x);
    current[i].y = wrap_add32(work_y, world_offset.y);
    current[i].z = wrap_add32(work_z, world_offset.z);
    previous[i] = current[i];

    if (travel != 0) {
      int16_t wheel_heading = car->car_velocity_heading_offset;
      if (front_heading_offset != 0 && i < 2) {
        wheel_heading = wrap_sub(car->car_velocity_heading_offset, front_heading_offset);
      }
      // Reduced model: ground is an infinite flat plane, so transform_wheel_
      // travel_to_world uses the no-plane branch:
      //   world_travel = car_to_world * R_y(-heading) * (0, 0, travel).
      fixed::Mat3 heading_rotation;
      fixed::mat_rot_y(wrap_negate(wheel_heading), &heading_rotation);
      const fixed::Vec3 forward{0, 0, travel};
      fixed::Vec3 plane_direction;
      fixed::mat_mul_vector(forward, heading_rotation, &plane_direction);
      fixed::Vec3 world_travel;
      fixed::mat_mul_vector(plane_direction, car_to_world, &world_travel);
      current[i].x = wrap_add32(current[i].x, world_travel.x);
      current[i].y = wrap_add32(current[i].y, world_travel.y);
      current[i].z = wrap_add32(current[i].z, world_travel.z);
    }
  }

  // ---- Wheel contact resolution --------------------------------------------
  int16_t contact_distances[4];
  int front_surfaces = 0;
  int rear_surfaces = 0;

  for (int i = 0; i < kCarWheelCount; ++i) {
    // Sample the ground the wheel is over (world units, matching word coords).
    const int wheel_x = static_cast<int>(current[i].x >> kWorldToLongShift);
    const int wheel_z = static_cast<int>(current[i].z >> kWorldToLongShift);
    int wheel_y = static_cast<int>(current[i].y >> kWorldToLongShift);
    const GroundSample ground_sample = ground(wheel_x, wheel_z);
    int16_t distance = static_cast<int16_t>(wheel_y - ground_sample.height);

    if (distance > 0) {
      // Airborne: accelerate the wheel downward (one native step).
      car->car_wheel_vertical_speed[i] =
          wrap_add(car->car_wheel_vertical_speed[i], kGravityStep[i]);
      current[i].y = wrap_sub32(current[i].y, car->car_wheel_vertical_speed[i]);
      wheel_y = static_cast<int>(current[i].y >> kWorldToLongShift);
      distance = static_cast<int16_t>(wheel_y - ground_sample.height);
      if (distance > kContactDistanceLimit) {
        car->car_surfaceWhl[i] = kWheelContactNone;
      } else {
        car->car_surfaceWhl[i] = static_cast<int8_t>(ground_sample.surface);
      }
    } else {
      car->car_surfaceWhl[i] = static_cast<int8_t>(ground_sample.surface);
    }

    // Penetration: push the wheel back out of the ground.
    if (distance < 0) {
      current[i].y = wrap_sub32(current[i].y, static_cast<int32_t>(distance)
                                                    << kWorldToLongShift);
    }

    // The suspension spring sees the signed distance before the push-out
    // correction; a landing (<= 0 after push-out) leaves the wheel still.
    contact_distances[i] = distance;
    if (distance <= 0) {
      car->car_wheel_vertical_speed[i] = 0;
    }
    if (car->car_surfaceWhl[i] != kWheelContactNone) {
      if (i < 2) {
        ++front_surfaces;
      } else {
        ++rear_surfaces;
      }
    }
  }

  car->car_sumSurfFrontWheels = static_cast<int8_t>(front_surfaces);
  car->car_sumSurfRearWheels = static_cast<int8_t>(rear_surfaces);
  car->car_sumSurfAllWheels = static_cast<int8_t>(front_surfaces + rear_surfaces);

  // ---- Suspension: lift each wheel to its body-attachment point ------------
  for (int i = 0; i < kCarWheelCount; ++i) {
    fixed::Vec3 contact_position{static_cast<int16_t>(current[i].x >> kWorldToLongShift),
                                 static_cast<int16_t>(current[i].y >> kWorldToLongShift),
                                 static_cast<int16_t>(current[i].z >> kWorldToLongShift)};
    car->car_wheel_contact_positions[i] = contact_position;

    const int16_t deflection = update_wheel_suspension(car, i, contact_distances[i]);
    const int16_t offset_y = wrap_add(deflection, kSuspensionTravelLimit);
    if (work_roll != 0 || work_pitch != 0) {
      const fixed::Vec3 offset{0, offset_y, 0};
      fixed::Vec3 rotated_offset;
      fixed::mat_mul_vector(offset, car_to_world, &rotated_offset);
      current[i].x = wrap_add32(current[i].x, rotated_offset.x);
      current[i].y = wrap_add32(current[i].y, rotated_offset.y);
      current[i].z = wrap_add32(current[i].z, rotated_offset.z);
    } else {
      current[i].y = wrap_add32(current[i].y, offset_y);
    }
  }

  // ---- Derive pose from the lifted wheel positions -------------------------
  // Centroid (unclamped) and per-wheel offsets.
  int32_t pivot_x = (static_cast<int32_t>(current[0].x) + current[1].x + current[2].x +
                     current[3].x) >>
                    kWheelCentroidShift;
  int32_t pivot_y = (static_cast<int32_t>(current[0].y) + current[1].y + current[2].y +
                     current[3].y) >>
                    kWheelCentroidShift;
  int32_t pivot_z = (static_cast<int32_t>(current[0].z) + current[1].z + current[2].z +
                     current[3].z) >>
                    kWheelCentroidShift;

  fixed::Vec3 offsets[4];
  for (int i = 0; i < kCarWheelCount; ++i) {
    offsets[i].x = difference_word_wrap(current[i].x, pivot_x);
    offsets[i].y = difference_word_wrap(current[i].y, pivot_y);
    offsets[i].z = difference_word_wrap(current[i].z, pivot_z);
  }

  if (pivot_y < 0) pivot_y = 0;
  pivot_x = std::clamp(pivot_x, kWorldMinPosition, kWorldMaxPosition);
  pivot_z = std::clamp(pivot_z, kWorldMinPosition, kWorldMaxPosition);

  // Yaw from wheel layout, then bring offsets into car-local space.
  const int16_t lateral_delta =
      wheel_pair_delta(offsets[3].x, offsets[2].x, offsets[0].x, offsets[1].x);
  const int16_t longitudinal_delta =
      wheel_pair_delta(offsets[3].z, offsets[2].z, offsets[0].z, offsets[1].z);
  int16_t yaw = static_cast<int16_t>(
      polar_angle16(lateral_delta, wrap_negate(longitudinal_delta)) & 0x3FF);
  fixed::Mat3 rotation;
  fixed::mat_rot_y(yaw, &rotation);
  fixed::Vec3 rotated[4];
  for (int i = 0; i < kCarWheelCount; ++i) {
    fixed::mat_mul_vector(offsets[i], rotation, &rotated[i]);
  }

  // Pitch after aligning to the yaw frame.
  const int16_t pitch_delta =
      wheel_pair_delta(rotated[3].z, rotated[2].z, rotated[0].z, rotated[1].z);
  const int16_t pitch_vert =
      wheel_pair_delta(rotated[3].y, rotated[2].y, rotated[0].y, rotated[1].y);
  int16_t pitch = 0;
  if (pitch_vert != 0 || pitch_delta >= 0) {
    pitch = apply_rotation_deadband(static_cast<int16_t>(
        wrap_sub(polar_angle16(wrap_negate(pitch_delta), pitch_vert), 0x100)));
  }
  if (pitch != 0) {
    fixed::mat_rot_x(pitch, &rotation);
    for (int i = 0; i < kCarWheelCount; ++i) {
      fixed::mat_mul_vector(rotated[i], rotation, &rotated[i]);
    }
  }

  // Roll from the pitch-corrected offsets.
  const int16_t roll_lateral =
      wheel_pair_delta(rotated[1].x, rotated[2].x, rotated[0].x, rotated[3].x);
  const int16_t roll_vert =
      wheel_pair_delta(rotated[1].y, rotated[2].y, rotated[0].y, rotated[3].y);
  int16_t roll = 0;
  if (roll_vert != 0 || roll_lateral <= 0) {
    roll = apply_rotation_deadband(static_cast<int16_t>(
        wrap_sub(polar_angle16(roll_lateral, roll_vert), 0x100)));
  }

  // ---- Commit --------------------------------------------------------------
  car->car_previous_position = car->car_position;
  car->car_position.x = pivot_x;
  car->car_position.y = pivot_y;
  car->car_position.z = pivot_z;
  car->car_rotate.x = yaw;
  car->car_rotate.y = pitch;
  car->car_rotate.z = roll;
}

void step_car_motion(CarState* car, const CarSpec& spec, int frames_per_sec,
                     const GroundSampler& ground) {
  step_car_motion(car, spec, frames_per_sec, ground, nullptr);
}

}  // namespace s3d::physics