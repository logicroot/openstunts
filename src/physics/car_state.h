#pragma once
// Player/opponent car state, mirroring the original `struct CARSTATE`
// (restunts gamestate.h). Fields keep the original names, widths and 16-bit
// wraparound semantics so the physics can be ported 1:1.
//
// `car_position` is in 1/64 world units (six fractional bits); the renderer
// shifts right by 6 (`position_to_word`). Rotation is in 1/1024 turn units.

#include <cstdint>

#include "physics/fixed_math.h"

namespace s3d::physics {

inline constexpr int kCarWheelCount = 4;

// 32-bit world position in 1/64 units.
struct Vec3Long {
  int32_t x = 0, y = 0, z = 0;
};

// Input flags (restunts game_input.h).
enum : uint8_t {
  kInputNone = 0,
  kInputAccelerate = 1,
  kInputBrake = 2,
  kInputSteerRight = 4,
  kInputSteerLeft = 8,
  kInputPrimaryAction = 16,    // shift up
  kInputSecondaryAction = 32,  // shift down
};
inline constexpr uint8_t kInputPedalMask = kInputAccelerate | kInputBrake;

// Gameplay constants (restunts gamestate.h / statecar.c).
inline constexpr int kTransmissionManual = 0;
inline constexpr int kTransmissionAutomatic = 1;
inline constexpr int kGearChangeInactive = 0;
inline constexpr int kGearChangeActive = 1;
inline constexpr int kGearChangeDelayExpired = 0;
inline constexpr int kPedalReleased = 0;
inline constexpr int kPedalPressed = 1;
inline constexpr int kSpeedStopped = 0;
inline constexpr int kWheelContactNone = 0;
inline constexpr int kGameFrameRateLow = 10;
inline constexpr int kGameFrameRateNormal = 20;

// Car surface types (restunts gamestate.h).
inline constexpr int kCarSurfacePaved = 1;
inline constexpr int kCarSurfaceDirt = 2;
inline constexpr int kCarSurfaceIce = 3;
inline constexpr int kCarSurfaceGrass = 4;
inline constexpr int kCarSurfaceWater = 5;

inline constexpr int kCarSteeringCentered = 0;
inline constexpr int kCarSlidingInactive = 0;
inline constexpr int kCarSlidingActive = 1;
inline constexpr int kCarCollisionLatchClear = 0;
inline constexpr int kCarCollisionLatchSet = 1;

// Grip/slip behaviour selection (restunts state_internal.h).
inline constexpr int kGripBehaviorOpponent = 0;
inline constexpr int kGripBehaviorPlayer = 1;

// Crash events (restunts crash_state.h). The reduced motion model triggers
// kCrashEventCollision on a hard wall hit (car_crashBmpFlag); the other values
// are kept for state-field compatibility.
inline constexpr int kCrashEventNone = 0;
inline constexpr int kCrashEventCollision = 1;
inline constexpr int kCrashEventWater = 2;
// update_crash_state(3, MplayerFlag): driving off the course band (lap 1+).
inline constexpr int kCrashEventOffTrack = 3;

// Per-frame car sound flags (restunts gamestate.h).
inline constexpr int kCarSoundNone = 0;
inline constexpr int kCarSoundEngineActive = 1;
inline constexpr int kCarSoundSkidPaved = 2;
inline constexpr int kCarSoundSkidOffroad = 4;

struct CarState {
  Vec3Long car_position;
  Vec3Long car_previous_position;
  fixed::Vec3 car_rotate;  // x = yaw... actually roll/pitch/yaw per original
  int16_t car_pseudoGravity = 0;
  int16_t car_steeringAngle = 0;
  int16_t car_currpm = 0;
  int16_t car_lastrpm = 0;
  int16_t car_initial_rpm = 0;
  int16_t car_speeddiff = 0;
  uint16_t car_rev_speed = 0;
  uint16_t car_actual_speed = 0;
  uint16_t car_lastspeed = 0;
  uint16_t car_gearratio = 0;
  uint16_t car_gearratioshr8 = 0;
  int16_t car_knob_x = 0;
  int16_t car_velocity_heading_offset = 0;
  int16_t car_knob_y = 0;
  int16_t car_knob_x2 = 0;
  int16_t car_knob_y2 = 0;
  int16_t car_slide_yaw_delta = 0;
  int16_t car_front_wheel_response_angle = 0;
  int16_t car_slip_angle = 0;
  int16_t car_demandedGrip = 0;
  int16_t car_surfacegrip_sum = 0;
  int16_t car_route_heading_error = 0;
  int16_t car_route_index = 0;
  int16_t car_wheel_vertical_speed[kCarWheelCount] = {0};
  int16_t car_suspension_deflection[kCarWheelCount] = {0};
  int16_t car_reserved_wheel_state[kCarWheelCount] = {0};
  int16_t car_reserved_contact_state[kCarWheelCount] = {0};
  int16_t car_suspension_target[kCarWheelCount] = {0};
  fixed::Vec3 car_wheel_contact_positions[kCarWheelCount];
  fixed::Vec3 car_body_corner_positions[kCarWheelCount];
  fixed::Vec3 car_route_target;
  fixed::Vec3 car_route_first_edge;
  fixed::Vec3 car_route_second_edge;
  int16_t car_route_has_reverse_path = 0;
  int16_t car_reserved_route_word1 = 0;
  int16_t car_reserved_route_word2 = 0;
  int8_t car_is_braking = 0;
  int8_t car_is_accelerating = 0;
  int8_t car_current_gear = 0;
  int8_t car_sumSurfFrontWheels = 0;
  int8_t car_sumSurfRearWheels = 0;
  int8_t car_sumSurfAllWheels = 0;
  int8_t car_surfaceWhl[kCarWheelCount] = {0};
  int8_t car_engineLimiterTimer = 0;
  int8_t car_slidingFlag = 0;
  int8_t car_collision_latch = 0;
  int8_t car_crashBmpFlag = 0;
  int8_t car_changing_gear = 0;
  int8_t car_gear_change_delay = 0;
  int8_t car_transmission = kTransmissionAutomatic;
  int8_t car_lap_count = 0;
  int8_t car_route_point_index = 0;
  int8_t car_route_flag = 0;  // field_CF (opponent_op's steering-nav latch)
  int8_t car_sound_flags = 0;
};

}  // namespace s3d::physics
