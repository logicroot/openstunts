#pragma once
// Car performance data (CAR*.RES -> "simd" chunk).
//
// Byte layout from restunts: externs.h (struct SIMD) and resfile/carinfo.h.
// All 16-bit values are little-endian signed/unsigned as noted. The original
// engine uses 16-bit integers; keep that in mind when porting physics.
//
// The "simd" chunk is always 776 bytes.

#include <cstdint>
#include <string>

namespace s3d {

struct Point2D {
  int16_t x = 0;
  int16_t y = 0;
};

struct Vector3i16 {
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
};

struct CarSpec {
  uint8_t num_gears = 0;
  uint8_t simd_unk = 0;
  int16_t car_mass = 0;
  int16_t braking_eff = 0;
  int16_t idle_rpm = 0;
  int16_t downshift_rpm = 0;
  int16_t upshift_rpm = 0;
  int16_t max_rpm = 0;
  uint16_t gear_ratios[7] = {0};
  Point2D knob_points[7] = {};

  int16_t aero_resistance = 0;
  // Precomputed drag lookup used by the speed model:
  //   aero_resistance_table[i] = ((aero_resistance * i) * i) >> 9
  // (restunts carsetup.c `aero_resistance_at`, 16-bit wrapping arithmetic).
  int16_t aero_resistance_table[64] = {0};
  uint8_t idle_torque = 0;
  uint8_t torque_curve[104] = {0};
  uint8_t field_A3 = 0;

  int16_t grip = 0;
  int16_t field_A6[7] = {0};

  // surfacegrip[0] = "sliding" (air/oversteer), [1..4] = tarmac/dirt/ice/grass.
  int16_t sliding = 0;
  int16_t surface_grip[4] = {0};

  // The word that follows surfacegrip[3] in the SIMD struct (offset 190). The
  // engine indexes its `sliding_values` array by surface id, so surface 5
  // (water) reads this neighbouring word rather than wrapping to grass.
  int16_t sliding_water = 0;

  Point2D collide_points[2] = {};
  int16_t car_height = 0;
  Vector3i16 wheel_coords[4] = {};

  // Human-readable names from the other chunks.
  std::string description;   // "edes" chunk (]]]-separated text)
  std::string driver_name;   // "gnam" chunk (player name)
  std::string opponent_name; // "gsna" chunk

  // Parses the 776-byte "simd" payload. Returns false if too small.
  bool parse_simd(const uint8_t* data, size_t size);

  // Full load from a CAR*.RES file (fills names too).
  static bool load(const std::string& path, CarSpec* out);
};

// The retail car resource file names (11 cars).
const char* const* retail_car_files(int* count);

}  // namespace s3d
