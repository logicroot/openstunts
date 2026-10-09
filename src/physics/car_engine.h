#pragma once
// Engine, gearbox and longitudinal speed model, ported from restunts
// `statecar.c` (update_car_speed). Operates in the original 16-bit fixed-point
// units; see car_state.h for the state layout.

#include <cstdint>

#include "assets/car.h"
#include "physics/car_state.h"

namespace s3d::physics {

// Advances one physics tick. `input_flags` uses the kInput* bits; pedals are
// the low two bits. `frames_per_sec` is the current tick rate (10 or 20).
// `is_opponent` enables the opponent drag governor (needs `opponent_target_speed`,
// 0..255). `top_speed` accumulates the highest actual speed seen.
void update_car_speed(uint8_t input_flags, const CarSpec& spec, CarState* car,
                      int frames_per_sec, bool is_opponent = false,
                      uint8_t opponent_target_speed = 200, uint16_t* top_speed = nullptr);

}  // namespace s3d::physics
