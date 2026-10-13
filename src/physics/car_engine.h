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

// True while the cockpit gear-selector plate ("gbox") and its lever knob should
// be on screen. The original draws the pair while a shift is in progress and
// keeps them up for a further `gear_change_delay` (1.5 s) afterwards; only once
// both `car_changing_gear` and `car_fpsmul2` are zero does seg005.asm loc_23030
// restore the saved background and blank the plate.
bool gear_box_visible(const CarState& car);

}  // namespace s3d::physics
