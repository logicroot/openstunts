#include "physics/car_collision.h"

namespace s3d::physics {
namespace {

// dseg.asm word_3BE04 / word_3BE0C: for each of the four box corners, whether
// the local x / z coordinate is negated.
constexpr int16_t kCornerNegX[4] = {1, 0, 0, 1};  // word_3BE04
constexpr int16_t kCornerNegZ[4] = {0, 0, 1, 1};  // word_3BE0C

// 16-bit signed |a - b| as the original computes it (compare then subtract).
inline int16_t abs_word_diff(int16_t a, int16_t b) {
  return a >= b ? static_cast<int16_t>(a - b) : static_cast<int16_t>(b - a);
}

// Builds the four box corners in world space: corner = R(-rot) * local + pos,
// where local.x = +/- points[0].x, local.y = 0, local.z = +/- points[1].x.
void build_corners(const CollisionPoints& points, const CollisionFrame& frame,
                   fixed::Vec3 out[4]) {
  const fixed::Mat3 mat =
      fixed::mat_rot_zxy(-frame.rot[0], -frame.rot[1], -frame.rot[2], 0);
  for (int i = 0; i < 4; ++i) {
    fixed::Vec3 local;
    local.x = kCornerNegX[i] ? static_cast<int16_t>(-points.w[0]) : points.w[0];
    local.y = 0;
    local.z = kCornerNegZ[i] ? static_cast<int16_t>(-points.w[2]) : points.w[2];
    fixed::Vec3 rot;
    fixed::mat_mul_vector(local, mat, &rot);
    out[i].x = fixed::wrap16(rot.x + frame.pos[0]);
    out[i].y = fixed::wrap16(rot.y + frame.pos[1]);
    out[i].z = fixed::wrap16(rot.z + frame.pos[2]);
  }
}

// Tests a world-space corner against `frame`/`points`' box after rotating
// (frame.pos - corner) into that car's local frame with the form-B matrix.
bool corner_inside(const fixed::Vec3& corner, const CollisionFrame& frame,
                   const CollisionPoints& points) {
  const fixed::Mat3 mat = fixed::mat_rot_zxy(frame.rot[0], frame.rot[1], frame.rot[2], 1);
  fixed::Vec3 in;
  in.x = fixed::wrap16(frame.pos[0] - corner.x);
  in.y = fixed::wrap16(frame.pos[1] - corner.y);
  in.z = fixed::wrap16(frame.pos[2] - corner.z);
  fixed::Vec3 q;
  fixed::mat_mul_vector(in, mat, &q);

  const int16_t neg_x = static_cast<int16_t>(-points.w[0]);
  const int16_t neg_z = static_cast<int16_t>(-points.w[2]);
  if (q.y < 0) return false;
  if (points.w[1] < q.y) return false;
  if (q.x < neg_x) return false;
  if (q.x > points.w[0]) return false;
  if (neg_z > q.z) return false;
  if (q.z > points.w[2]) return false;
  return true;
}

}  // namespace

CollisionPoints collision_points_from_spec(const CarSpec& spec) {
  CollisionPoints points;
  points.w[0] = spec.collide_points[0].x;
  points.w[1] = spec.collide_points[0].y;
  points.w[2] = spec.collide_points[1].x;
  points.w[3] = spec.collide_points[1].y;
  return points;
}

CollisionFrame make_collision_frame(const CarState& car) {
  CollisionFrame f;
  f.pos[0] = static_cast<int16_t>(car.car_position.x >> 6);
  f.pos[1] = static_cast<int16_t>(car.car_position.y >> 6);
  f.pos[2] = static_cast<int16_t>(car.car_position.z >> 6);
  // stateply.c: (car_rotate.z, car_rotate.y, car_rotate.x), for either car.
  f.rot[0] = car.car_rotate.z;
  f.rot[1] = car.car_rotate.y;
  f.rot[2] = car.car_rotate.x;
  return f;
}

bool car_car_collide(const CollisionPoints& p_points, const CollisionFrame& p_frame,
                     const CollisionPoints& o_points, const CollisionFrame& o_frame) {
  // Coarse per-axis reject using si = points[1].y (of both cars) as half-extent.
  const int16_t si = fixed::wrap16(p_points.w[3] + o_points.w[3]);
  if (abs_word_diff(p_frame.pos[0], o_frame.pos[0]) > si) return false;
  if (abs_word_diff(p_frame.pos[2], o_frame.pos[2]) > si) return false;
  if (abs_word_diff(p_frame.pos[1], o_frame.pos[1]) > si) return false;

  // Bounding-sphere reject (unsigned compare, as the original's `ja`).
  fixed::Vec3 delta;
  delta.x = fixed::wrap16(p_frame.pos[0] - o_frame.pos[0]);
  delta.y = fixed::wrap16(p_frame.pos[1] - o_frame.pos[1]);
  delta.z = fixed::wrap16(p_frame.pos[2] - o_frame.pos[2]);
  const auto radius = static_cast<uint16_t>(fixed::polar_radius3d(delta));
  if (radius > static_cast<uint16_t>(si)) return false;

  // Player corners against the opponent's box...
  fixed::Vec3 p_corners[4];
  build_corners(p_points, p_frame, p_corners);
  for (int i = 0; i < 4; ++i) {
    if (corner_inside(p_corners[i], o_frame, o_points)) return true;
  }

  // ...then opponent corners against the player's box.
  fixed::Vec3 o_corners[4];
  build_corners(o_points, o_frame, o_corners);
  for (int i = 0; i < 4; ++i) {
    if (corner_inside(o_corners[i], p_frame, p_points)) return true;
  }
  return false;
}

bool car_car_speed_adjust(CarState* self, CarState* other) {
  // seg001.asm car_car_speed_adjust_maybe: arg_oState is the car being ticked
  // (self), arg_pState is the other car.
  self->car_collision_latch = kCarCollisionLatchSet;
  other->car_collision_latch = kCarCollisionLatchSet;

  const uint16_t self_speed2 = self->car_actual_speed;   // car_speed2
  const uint16_t other_speed2 = other->car_actual_speed;  // car_speed2
  const int16_t self_yaw = self->car_rotate.x;
  const int16_t other_yaw = other->car_rotate.x;

  // Velocity vectors v = (speed2 >> 8) * (sin yaw, cos yaw).
  const int16_t self_sin = fixed::sin_fast(static_cast<unsigned short>(self_yaw));
  const int16_t other_sin = fixed::sin_fast(static_cast<unsigned short>(other_yaw));
  const int16_t self_cos = fixed::cos_fast(static_cast<unsigned short>(self_yaw));
  const int16_t other_cos = fixed::cos_fast(static_cast<unsigned short>(other_yaw));
  const int16_t vx_self = fixed::multiply_and_scale(self_sin, static_cast<int16_t>(self_speed2 >> 8));
  const int16_t vx_other =
      fixed::multiply_and_scale(other_sin, static_cast<int16_t>(other_speed2 >> 8));
  const int16_t vz_self = fixed::multiply_and_scale(self_cos, static_cast<int16_t>(self_speed2 >> 8));
  const int16_t vz_other =
      fixed::multiply_and_scale(other_cos, static_cast<int16_t>(other_speed2 >> 8));

  int rel = fixed::polar_radius2d(static_cast<int>(fixed::wrap16(vx_other - vx_self)),
                                  static_cast<int>(fixed::wrap16(vz_other - vz_self)));
  if (rel < 0xA) rel = 0xA;

  // car_speed2 -= (0x300 * rel) >> 2 (16-bit arithmetic), clamped at zero.
  const int16_t decel = static_cast<int16_t>((fixed::wrap16(0x300 * rel)) >> 2);
  if (self_speed2 >= static_cast<uint16_t>(decel)) {
    self->car_actual_speed = static_cast<uint16_t>(self_speed2 - static_cast<uint16_t>(decel));
  } else {
    self->car_actual_speed = kSpeedStopped;
  }

  // Heading kick: each car is turned away from the other, normalized to
  // (-0x200, 0x200].
  self->car_velocity_heading_offset = fixed::wrap16(other_yaw - self_yaw);
  if (self->car_velocity_heading_offset >= 0x200) {
    self->car_velocity_heading_offset =
        fixed::wrap16(self->car_velocity_heading_offset - 0x400);
  }
  if (self->car_velocity_heading_offset <= static_cast<int16_t>(0xFE00)) {
    self->car_velocity_heading_offset =
        fixed::wrap16(self->car_velocity_heading_offset + 0x400);
  }

  other->car_velocity_heading_offset = fixed::wrap16(self_yaw - other_yaw);
  if (other->car_velocity_heading_offset >= 0x200) {
    other->car_velocity_heading_offset =
        fixed::wrap16(other->car_velocity_heading_offset - 0x400);
  }
  if (other->car_velocity_heading_offset <= static_cast<int16_t>(0xFE00)) {
    other->car_velocity_heading_offset =
        fixed::wrap16(other->car_velocity_heading_offset + 0x400);
  }

  self->car_rev_speed = self->car_actual_speed;     // car_speed  = car_speed2
  other->car_rev_speed = other->car_actual_speed;
  return rel > 0x1E;
}

bool car_car_resolve(CarState* self, const CollisionPoints& self_points,
                     CarState* other, const CollisionPoints& other_points) {
  const CollisionFrame self_frame = make_collision_frame(*self);
  const CollisionFrame other_frame = make_collision_frame(*other);

  if (!car_car_collide(self_points, self_frame, other_points, other_frame)) {
    // loc_16840: a clear tick drops the contact latch.
    self->car_collision_latch = kCarCollisionLatchClear;
    return false;
  }

  // loc_164B2: an overlap. `field_C8` gates processing to the first tick of a
  // contact, so the opponent's pass does not re-apply what the player's did.
  if (self->car_collision_latch != kCarCollisionLatchClear) {
    return true;  // loc_16892: leave the latch set.
  }

  if (car_car_speed_adjust(self, other)) {
    // loc_16550: update_crash_state(1, MplayerFlag) and (1, MplayerFlag ^ 1).
    // update_crash_state keeps an existing crash state.
    if (self->car_crashBmpFlag == kCrashEventNone) {
      self->car_crashBmpFlag = kCrashEventCollision;
    }
    if (other->car_crashBmpFlag == kCrashEventNone) {
      other->car_crashBmpFlag = kCrashEventCollision;
    }
  }
  return true;
}

}  // namespace s3d::physics
