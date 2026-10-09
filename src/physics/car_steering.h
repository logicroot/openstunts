#pragma once
// Steering input/response and grip/slip handling, ported from restunts
// `opponent.c` (update_player_steering) and `physics_grip.c` (update_grip).
//
// The port runs the player behaviour only (GRIP_BEHAVIOR_PLAYER); the opponent
// steering path is not used yet.

#include <cstdint>

#include "assets/car.h"
#include "physics/car_state.h"

namespace s3d::physics {

// Applies one steering tick from `input_flags` (kInputSteer{Right,Left} bits).
// `frames_per_sec` selects the 10 or 20 Hz response table.
void update_player_steering(uint8_t input_flags, CarState* car, int frames_per_sec);

// Grip and slip update. Called after update_car_speed, before car motion; uses
// the wheel surface ids refreshed by the previous motion tick.
void update_grip(CarState* car, const CarSpec& spec);

// Signed arithmetic right shift on the low 16 bits (LEGACY_S16_SAR). Negative
// (left) angles round down in magnitude, which is the original left-corner
// bias (restunts physics_grip.c scale_cornering_angle).
int16_t scale_cornering_angle(int16_t angle, int shift);

// (angle * 15) >> 4 with 16-bit wrapping multiply (restunts
// damp_collision_angle, physics_collision.c).
int16_t damp_collision_angle(int16_t angle);

}  // namespace s3d::physics