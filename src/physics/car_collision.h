#pragma once
// Car-vs-car collision, ported from the original engine.
//
//   * car_car_collide      <- car_car_coll_detect_maybe (seg001.asm:8124-8528)
//   * car_car_speed_adjust <- car_car_speed_adjust_maybe (seg001.asm:6689-6862)
//
// The original implements an approximate box test: each car's SIMD
// `collide_points` define a flat rectangle (half-width `points[0].x`, half-depth
// `points[1].x`) extruded upward from 0 to `points[0].y` in the car's local
// frame. The test is asymmetric-but-symmetric: the player's four rotated
// corners are tested against the opponent's box and vice versa.
//
// Reference: reference/restunts/src/restunts/asmorig/seg001.asm and
// reference/restunts/src/restunts/c/stateply.c (caller frame construction).

#include <cstdint>

#include "physics/car_state.h"
#include "physics/fixed_math.h"

namespace s3d::physics {

// A car's `collide_points`, as four 16-bit words: p0.x, p0.y, p1.x, p1.y.
struct CollisionPoints {
  int16_t w[4] = {0, 0, 0, 0};
};

// The two-VECTOR world frame the original passes to the collision test:
//   pos = car_posWorld1 >> 6 (x, y, z)
//   rot = the caller-stored rotation vector
// The player stores (-rotate.z, -rotate.x, -rotate.y); the opponent stores
// (rotate.z, rotate.y, rotate.x). See make_collision_frame().
struct CollisionFrame {
  int16_t pos[3] = {0, 0, 0};
  int16_t rot[3] = {0, 0, 0};
};

// Builds the frame exactly as stateply.c does: the player negates and reorders
// its rotation, the opponent stores it verbatim.
CollisionFrame make_collision_frame(const CarState& car, bool is_player);

// seg001.asm car_car_coll_detect_maybe: true when the boxes overlap.
bool car_car_collide(const CollisionPoints& p_points, const CollisionFrame& p_frame,
                     const CollisionPoints& o_points, const CollisionFrame& o_frame);

// seg001.asm car_car_speed_adjust_maybe: called as (self, other) where `self`
// is the car being ticked. Applies the speed loss / heading kick from a
// collision and returns true when it is hard enough to trigger a crash.
bool car_car_speed_adjust(CarState* self, CarState* other);

}  // namespace s3d::physics
