#pragma once
// Fixed-point math ported from the original engine (restunts math.c).
//
// Conventions:
//   * Angles use a 0x400 (1024) unit full circle: 0x100 = 90 degrees.
//   * sin/cos return 1.14 fixed point (0x4000 = 1.0).
//   * Matrices are 3x3 in 1.14 fixed point (0x4000 = 1.0). Products are
//     shifted right by 14 and accumulated in 16-bit (with wraparound) to match
//     the original DOS behaviour exactly.
//
// The original engine stores matrices/vectors/logic values in 16-bit `int`s;
// here we use int16_t explicitly so overflow semantics are preserved.

#include <cstdint>

namespace s3d::fixed {

inline constexpr int kAngleFull = 0x400;   // 360 degrees
inline constexpr int kAngleQuarter = 0x100; // 90 degrees
inline constexpr int kOne = 0x4000;        // 1.0 in 1.14

struct Vec3 {
  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
};

// Row-major 3x3 matrix (m[0..8] == m11,m12,m13,m21,...).
struct Mat3 {
  int16_t m[9] = {kOne, 0, 0, 0, kOne, 0, 0, 0, kOne};

  int16_t& at(int row, int col) { return m[row * 3 + col]; }
  int16_t at(int row, int col) const { return m[row * 3 + col]; }
};

// 1.14 sine/cosine (magnitude 0x4000).
int16_t sin_fast(unsigned short angle);
int16_t cos_fast(unsigned short angle);

// Angle of the vector (z, y) in 0x400 units. Returns 0 for a zero vector
// (the original returns an uninitialized value in that case).
int polar_angle(int z, int y);

// Magnitude of (z, y) / for a Vec3.
int polar_radius2d(int z, int y);
int polar_radius3d(const Vec3& v);

// out = mat * in (1.14).
void mat_mul_vector(const Vec3& in, const Mat3& mat, Vec3* out);

// out = rmat * lmat (1.14, per-term shift + 16-bit accumulation).
void mat_multiply(const Mat3& rmat, const Mat3& lmat, Mat3* out);

void mat_invert(const Mat3& in, Mat3* out);
void mat_rot_x(int angle, Mat3* out);
void mat_rot_y(int angle, Mat3* out);
void mat_rot_z(int angle, Mat3* out);
// Z then X then Y (or Y*X*Z when `unk & 1`), matching mat_rot_zxy.
Mat3 mat_rot_zxy(int z, int x, int y, int unk);

// Rounded 1.14 multiply: ((a*b*4) >> 16) + (((a*b*4) & 0x8000) >> 15).
int16_t multiply_and_scale(int16_t a, int16_t b);

// (normal.x*x + normal.z*z + normal.y*y) / 0x2000.
int vec_normal_inner_product(int x, int y, int z, const Vec3& normal);

// Wraps an int32 into int16 the way the original 16-bit accumulator would.
inline int16_t wrap16(int32_t v) {
  return static_cast<int16_t>(static_cast<uint16_t>(v & 0xFFFF));
}

}  // namespace s3d::fixed
