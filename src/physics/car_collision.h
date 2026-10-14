#pragma once
// Car-vs-car collision, ported from the original engine.
//
//   * car_car_collide      <- car_car_coll_detect_maybe (seg001.asm:8124-8528)
//   * car_car_speed_adjust <- car_car_speed_adjust_maybe (seg001.asm:6689-6862)
//   * car_car_resolve      <- the collision branch of update_player_state
//                             (seg001.asm loc_164B2..loc_16550)
//
// The original implements an approximate box test: each car's SIMD
// `collide_points` define a flat rectangle (half-width `points[0].x`, half-depth
// `points[1].x`) extruded upward from 0 to `points[0].y` in the car's local
// frame. The test is symmetric: the ticked car's four rotated corners are tested
// against the other car's box and vice versa.
//
// Reference: reference/restunts/src/restunts/asmorig/seg001.asm and
// reference/restunts/src/restunts/c/stateply.c (caller frame construction).

#include <cstdint>

#include "assets/car.h"
#include "physics/car_state.h"
#include "physics/fixed_math.h"

namespace s3d::physics {

// A car's `collide_points`, as four 16-bit words: p0.x, p0.y, p1.x, p1.y.
struct CollisionPoints {
  int16_t w[4] = {0, 0, 0, 0};
};

// Builds the four-word box from a car profile's SIMD collide_points.
CollisionPoints collision_points_from_spec(const CarSpec& spec);

// The two-VECTOR world frame the original passes to the collision test:
//   pos = car_posWorld1 >> 6 (x, y, z)
//   rot = (car_rotate.z, car_rotate.y, car_rotate.x)
// Both frames use the same layout: stateply.c rebuilds var_11ApStateWorldCrds
// and vec_18EoStateWorldCrds from car_rotate.vz/vy/vx unchanged, regardless of
// which car is in the p / o slot.
struct CollisionFrame {
  int16_t pos[3] = {0, 0, 0};
  int16_t rot[3] = {0, 0, 0};
};

// Builds the frame exactly as stateply.c does.
CollisionFrame make_collision_frame(const CarState& car);

// seg001.asm car_car_coll_detect_maybe: true when the boxes overlap.
bool car_car_collide(const CollisionPoints& p_points, const CollisionFrame& p_frame,
                     const CollisionPoints& o_points, const CollisionFrame& o_frame);

// seg001.asm car_car_speed_adjust_maybe: called as (self, other) where `self`
// is the car being ticked. Applies the speed loss / heading kick from a
// collision and returns true when it is hard enough to trigger a crash.
bool car_car_speed_adjust(CarState* self, CarState* other);

// The car-vs-car branch of update_player_state (seg001.asm loc_164B2..loc_16550)
// for the car being ticked (`self`) against `other`. Mirrors the original's
// per-contact latch (field_C8) exactly:
//   * no overlap: clears self's car_collision_latch (loc_16840), returns false;
//   * overlap with the latch clear: car_car_speed_adjust (self loses speed,
//     both take the heading kick) and, on a hard enough hit, both cars enter
//     the collision crash state (update_crash_state(1, MplayerFlag) and
//     (1, MplayerFlag ^ 1));
//   * overlap with the latch already set: nothing -- the episode was already
//     handled by the other car's earlier pass.
// Returns true when the boxes overlapped.
bool car_car_resolve(CarState* self, const CollisionPoints& self_points,
                     CarState* other, const CollisionPoints& other_points);

}  // namespace s3d::physics
