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

// Ported from restunts math.c `vector_op_unk` with i == 0: the point where the
// segment (a -> b) crosses z == 0 (used for the swept wheel/wall test).
fixed::Vec3 segment_cross_z0(fixed::Vec3 a, fixed::Vec3 b) {
  fixed::Vec3 out{};
  out.z = 0;
  int32_t var_4 = 0 - static_cast<int32_t>(b.z);
  int32_t var_2 = static_cast<int32_t>(a.z) - static_cast<int32_t>(b.z);
  if (var_2 < 0) {
    var_4 >>= 1;
    var_2 >>= 1;
  }
  if (var_2 != 0) {
    out.x = static_cast<int16_t>((static_cast<int32_t>(a.x) - b.x) * var_4 / var_2 + b.x);
    out.y = static_cast<int16_t>((static_cast<int32_t>(a.y) - b.y) * var_4 / var_2 + b.y);
  } else {
    out.x = b.x;
    out.y = b.y;
  }
  return out;
}

}  // namespace

WallContact resolve_wall_contact(const CollisionSample& sample, int plane_distance,
                                 const fixed::Vec3& pre_word, const fixed::Vec3& post_word,
                                 int16_t travel, int16_t yaw, int16_t pitch, int16_t roll) {
  WallContact contact;
  if (sample.wall_index < 0 || travel == 0) return contact;
  // Band test (stateply.c loc_15264..loc_15270): the wheel must sit inside the
  // wall's [lower_bound, height) slice of the plane distance.
  if (plane_distance <= sample.wall_lower_bound || plane_distance >= sample.wall_height) {
    return contact;
  }

  // Wheel positions relative to the wall segment origin (x/z plane only).
  const fixed::Vec3 rel_pre{static_cast<int16_t>(pre_word.x - sample.wall_start_x), 0,
                            static_cast<int16_t>(pre_word.z - sample.wall_start_z)};
  const fixed::Vec3 rel_post{static_cast<int16_t>(post_word.x - sample.wall_start_x), 0,
                             static_cast<int16_t>(post_word.z - sample.wall_start_z)};
  fixed::Mat3 wall_frame;
  fixed::mat_rot_y(wrap_negate(sample.wall_orientation) - fixed::kAngleQuarter, &wall_frame);
  fixed::Vec3 c_pre, c_post;
  fixed::mat_mul_vector(rel_pre, wall_frame, &c_pre);
  fixed::mat_mul_vector(rel_post, wall_frame, &c_post);

  // Same side of the wall line at both ends: the wheel did not cross it.
  if ((c_pre.z > 0 && c_post.z > 0) || (c_pre.z < 0 && c_post.z < 0)) return contact;

  // Order by the across-wall coordinate. `swapped` mirrors `var_136`.
  bool swapped = false;
  fixed::Vec3 lo = c_post;
  fixed::Vec3 hi = c_pre;
  if (c_post.z > c_pre.z) {
    swapped = true;
    lo = c_pre;
    hi = c_post;
  }

  int32_t var_f2 = 0;  // penetration past the wall (var_F2)
  int32_t var_f4 = 0;  // travel that survives (var_pSpeed2Scaled - var_F2)
  if (lo.z == 0) {
    var_f4 = travel;
  } else if (hi.z == 0) {
    var_f2 = travel;
  } else {
    const fixed::Vec3 cross = segment_cross_z0(lo, hi);
    const fixed::Vec3 delta{static_cast<int16_t>(lo.x - cross.x),
                            static_cast<int16_t>(lo.y - cross.y),
                            static_cast<int16_t>(lo.z - cross.z)};
    const fixed::Vec3 delta6{static_cast<int16_t>(delta.x << kWorldToLongShift),
                             static_cast<int16_t>(delta.y << kWorldToLongShift),
                             static_cast<int16_t>(delta.z << kWorldToLongShift)};
    var_f2 = fixed::polar_radius3d(delta6);
    var_f4 = travel - var_f2;
  }

  // Slide direction along the wall (stateply.c loc_1540C..loc_1546E).
  int16_t var_ee = static_cast<int16_t>((wrap_negate(yaw) - sample.wall_orientation) & 0x3FF);
  int16_t deflection_x = 0;
  if (var_ee < 0x100 || var_ee > 0x300) {
    var_ee = sample.wall_orientation;
    deflection_x = 768;
  } else {
    var_ee = static_cast<int16_t>((sample.wall_orientation + 0x200) & 0x3FF);
    deflection_x = -768;
  }
  fixed::Vec3 wall_vec{deflection_x, 0, static_cast<int16_t>(var_f2)};
  if (swapped) wall_vec.x = wrap_negate(wall_vec.x);
  const fixed::Mat3 deflect_frame =
      fixed::mat_rot_zxy(wrap_negate(roll), wrap_negate(pitch), var_ee, 0);
  fixed::mat_mul_vector(wall_vec, deflect_frame, &contact.deflection);

  contact.hit = true;
  contact.remaining_travel = var_f4;
  contact.wall_frame_angle = var_ee;
  return contact;
}

WallCrash compute_wall_crash(int16_t yaw, int16_t wall_frame_angle, uint16_t actual_speed) {
  WallCrash crash;
  // loc_1546E: wall-relative heading folded into [0, 0x100]. `var_138` records
  // which side of the fold the angle landed on.
  int32_t si = static_cast<int32_t>(wrap_sub(wrap_negate(yaw), wall_frame_angle)) & 0x3FF;
  int32_t mirrored = 0;
  if (si > 0x100) {
    si = 0x400 - si;
    mirrored = 1;
  }

  // loc_154CA: var_190 = ((si * 0x46) sar 8 - 0x64) negated, packed into the
  // high byte. The original `imul` is 16-bit and wraps, so use wrap_mul.
  const int16_t product = wrap_mul(static_cast<int16_t>(si), 0x46);
  const int16_t scaled = sar16(product, 8);
  const int16_t biased = wrap_sub(scaled, 0x64);
  const int16_t negated = wrap_negate(biased);
  const uint16_t threshold = static_cast<uint16_t>(static_cast<uint16_t>(negated & 0xFF) << 8);
  if (actual_speed <= threshold) return crash;

  // loc_154F8..loc_154FA: car_36MwhlAngle = var_138 ? -(si << 1) : (si << 1).
  crash.triggered = true;
  const int16_t kick = static_cast<int16_t>(static_cast<uint16_t>(si) << 1);
  crash.heading_kick = mirrored != 0 ? wrap_negate(kick) : kick;
  return crash;
}

void apply_crash_control(const CarState& car, uint8_t* input_flags) {
  if (input_flags == nullptr || car.car_crashBmpFlag == kCrashEventNone) return;
  // player_op: `arg_carInputByte = 2` (brake) while car_crashBmpFlag != 0.
  *input_flags = kInputBrake;
}

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

  // When a loaded track collision model is bound, wheels ride the actual track
  // planes (ramps, banked roads, elevated terrain) instead of the caller's flat
  // 2D sampler. `sample_wheel_ground` resolves the element under the wheel,
  // then returns the wheel's signed distance to that plane; otherwise it falls
  // back to the 2D sampler.
  const bool use_track_collision =
      collision != nullptr && collision->is_loaded() && collision->track() != nullptr;
  auto sample_wheel_ground = [&](int wheel_x, int wheel_y, int wheel_z, int16_t* distance_out,
                                 int16_t* plane_out) -> int8_t {
    if (use_track_collision) {
      const fixed::Vec3 word{static_cast<int16_t>(wheel_x), static_cast<int16_t>(wheel_y),
                             static_cast<int16_t>(wheel_z)};
      CollisionSample sample;
      collision->build_track_object_from_track(*collision->track(), word, word, &sample);
      *distance_out = static_cast<int16_t>(collision->plane_signed_distance(
          sample.plane_index, wheel_x, wheel_y, wheel_z, sample.terrain_height,
          sample.elem_x_center, sample.elem_z_center));
      *plane_out = sample.plane_index;
      return sample.surface_type;
    }
    const GroundSample sampled = ground(wheel_x, wheel_z);
    *distance_out = static_cast<int16_t>(wheel_y - sampled.height);
    *plane_out = -1;  // the flat 2D sampler has no plane; push straight up
    return static_cast<int8_t>(sampled.surface);
  };

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
      fixed::Vec3 world_travel;
      // The reference builds each wheel's travel from the car's *own*
      // orientation: update_player_state pins planindex_copy to 0xFFFF just
      // before the wheel loop (seg001.asm loc_14F7C), so the plane_rotate_op
      // call at loc_14FA6 always takes its no-plane branch:
      //   world_travel = car_to_world * R_y(-heading) * (0, 0, travel)
      // The element plane under the wheel is only consulted afterwards, for the
      // contact distance/wall response. Projecting the travel onto that plane
      // instead strips the upward component while the car is still pitched up
      // off a ramp lip -- the "climbs the ramp but never flies" bug (the travel
      // gets clamped into the ground plane). Keep the car-frame travel.
      if (collision != nullptr) {
        collision->transform_wheel_travel_to_world(travel, wheel_heading, car_to_world, work_yaw,
                                                   work_pitch, work_roll, -1, &world_travel);
      } else {
        fixed::Mat3 heading_rotation;
        fixed::mat_rot_y(wrap_negate(wheel_heading), &heading_rotation);
        const fixed::Vec3 forward{0, 0, travel};
        fixed::Vec3 heading_dir;
        fixed::mat_mul_vector(forward, heading_rotation, &heading_dir);
        fixed::mat_mul_vector(heading_dir, car_to_world, &world_travel);
      }
      current[i].x = wrap_add32(current[i].x, world_travel.x);
      current[i].y = wrap_add32(current[i].y, world_travel.y);
      current[i].z = wrap_add32(current[i].z, world_travel.z);
    }
  }

  // ---- Wall collision response (stateply.c loc_15257..loc_15599) -----------
  // build_track_object resolves the active wall for each tile; here we sweep
  // each wheel across the wall line and, on a crossing inside the wall's height
  // band, push the wheels back along the wall. A hard enough hit also enters
  // the crash state (compute_wall_crash); the original then slows the car
  // through the forced-brake crash control rather than draining speed here.
  if (use_track_collision && travel != 0) {
    for (int i = 0; i < kCarWheelCount; ++i) {
      const fixed::Vec3 pre_word{static_cast<int16_t>(previous[i].x >> kWorldToLongShift),
                                 static_cast<int16_t>(previous[i].y >> kWorldToLongShift),
                                 static_cast<int16_t>(previous[i].z >> kWorldToLongShift)};
      const fixed::Vec3 post_word{static_cast<int16_t>(current[i].x >> kWorldToLongShift),
                                  static_cast<int16_t>(current[i].y >> kWorldToLongShift),
                                  static_cast<int16_t>(current[i].z >> kWorldToLongShift)};
      // The element/plane is resolved at the pre-travel position (matching the
      // reference's car_whlWorldCrds1); the wall sweep itself uses post.
      CollisionSample wall_sample;
      collision->build_track_object_from_track(*collision->track(), pre_word, post_word,
                                               &wall_sample);
      if (wall_sample.wall_index < 0) continue;
      const int plane_distance = collision->plane_signed_distance(
          wall_sample.plane_index, post_word.x, post_word.y, post_word.z,
          wall_sample.terrain_height, wall_sample.elem_x_center, wall_sample.elem_z_center);
      const WallContact contact =
          resolve_wall_contact(wall_sample, plane_distance, pre_word, post_word, travel, work_yaw,
                               work_pitch, work_roll);
      if (!contact.hit) continue;

      // Blend every wheel: pull it back along its travel by the blocked fraction
      // and add the wall deflection (stateply.c loc_155A1..loc_1553F).
      for (int w = 0; w < kCarWheelCount; ++w) {
        const int32_t dx = current[w].x - previous[w].x;
        const int32_t dy = current[w].y - previous[w].y;
        const int32_t dz = current[w].z - previous[w].z;
        int16_t corr_x = 0;
        int16_t corr_y = 0;
        int16_t corr_z = 0;
        if (contact.remaining_travel != 0) {
          corr_x = static_cast<int16_t>((dx * contact.remaining_travel) / travel);
          corr_y = static_cast<int16_t>((dy * contact.remaining_travel) / travel);
          corr_z = static_cast<int16_t>((dz * contact.remaining_travel) / travel);
        }
        current[w].x = previous[w].x + static_cast<int16_t>(corr_x + contact.deflection.x);
        current[w].y = previous[w].y + static_cast<int16_t>(corr_y + contact.deflection.y);
        current[w].z = previous[w].z + static_cast<int16_t>(corr_z + contact.deflection.z);
      }

      // Crash threshold (stateply.c loc_154CA..loc_154FA): a hard enough hit
      // kicks the wheel-travel heading and enters the crash state, which the
      // original advances through player_op's forced brake rather than by
      // draining speed here.
      const WallCrash crash =
          compute_wall_crash(work_yaw, contact.wall_frame_angle, car->car_actual_speed);
      if (crash.triggered) {
        car->car_velocity_heading_offset = crash.heading_kick;
        car->car_crashBmpFlag = kCrashEventCollision;
      }
      break;  // the reference resolves a single wall per tick
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
    int16_t distance = 0;
    int16_t plane = -1;
    int8_t surface = sample_wheel_ground(wheel_x, wheel_y, wheel_z, &distance, &plane);

    if (distance > 0) {
      // Airborne: accelerate the wheel downward (one native step), then re-test
      // the surface below the wheel (reference re-runs build_track_object and
      // plane_origin_op at the new position, stateply.c loc_16288).
      car->car_wheel_vertical_speed[i] =
          wrap_add(car->car_wheel_vertical_speed[i], kGravityStep[i]);
      current[i].y = wrap_sub32(current[i].y, car->car_wheel_vertical_speed[i]);
      wheel_y = static_cast<int>(current[i].y >> kWorldToLongShift);
      surface = sample_wheel_ground(wheel_x, wheel_y, wheel_z, &distance, &plane);
      if (distance > kContactDistanceLimit) {
        car->car_surfaceWhl[i] = kWheelContactNone;
      } else {
        car->car_surfaceWhl[i] = surface;
      }
    } else {
      car->car_surfaceWhl[i] = surface;
    }

    // Penetration: push the wheel back out of the ground along the active
    // plane's own up axis (seg001.asm loc_15C04). Flat planes carry an identity
    // rotation, so this is the plain vertical push there; on a slope it follows
    // the surface instead of leaving the vertical slice of the penetration
    // uncorrected.
    if (distance < 0) {
      if (plane >= 0 && collision != nullptr) {
        fixed::Vec3 push;
        collision->plane_push_out(plane, distance, &push);
        current[i].x = wrap_add32(current[i].x, push.x);
        current[i].y = wrap_add32(current[i].y, push.y);
        current[i].z = wrap_add32(current[i].z, push.z);
      } else {
        current[i].y = wrap_sub32(current[i].y, static_cast<int32_t>(distance)
                                                      << kWorldToLongShift);
      }
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

  // Water crash (stateply.c loc_15163, player_op): once every wheel rides the
  // water surface the car enters the sinking crash state. update_crash_state
  // early-returns when a flag is already set, so only a healthy car transitions
  // (a same-tick wall crash keeps flag 1). The original resolves the wall first
  // and the water second, which this ordering matches.
  if (car->car_crashBmpFlag == kCrashEventNone) {
    bool all_water = true;
    for (int i = 0; i < kCarWheelCount; ++i) {
      if (car->car_surfaceWhl[i] != kCarSurfaceWater) {
        all_water = false;
        break;
      }
    }
    if (all_water) car->car_crashBmpFlag = kCrashEventWater;
  }

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