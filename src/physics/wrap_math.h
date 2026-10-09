#pragma once
// 16/32-bit wrapping helpers matching the original engine's `legacy_s16_*`
// macros (restunts legacy.h). The DOS code keeps nearly every logic value in
// 16-bit registers and wraps on overflow; the physics ports must reproduce
// that behaviour bit-for-bit.

#include <cstdint>

namespace s3d::physics {

// (s16) result of an arithmetic shift right, matching LEGACY_S16_SAR: the sign
// bit is copied into the vacated bits, so negative values round toward minus
// infinity (LEGACY_U16_SAR fills 1s when the input sign bit is set). The shift
// count is masked to 4 bits like the original x86 `sar`/`shr` instructions.
inline int16_t sar16(int16_t v, int shift) {
  const int count = shift & 0xF;
  if (count == 0) return v;
  const uint16_t uv = static_cast<uint16_t>(v);
  if ((uv & 0x8000) != 0) {
    const uint16_t sign_fill =
        static_cast<uint16_t>(static_cast<uint16_t>(0xFFFF) << (16 - count));
    return static_cast<int16_t>(static_cast<uint16_t>((uv >> count) | sign_fill));
  }
  return static_cast<int16_t>(uv >> count);
}

// (s16) result of shifting the unsigned bits left (LEGACY_S16_SHL).
inline int16_t shl16(int16_t v, int shift) {
  return static_cast<int16_t>(static_cast<uint16_t>(v) << shift);
}

inline int16_t wrap_add(int16_t a, int16_t b) {
  return static_cast<int16_t>(static_cast<uint16_t>(a) + static_cast<uint16_t>(b));
}

inline int16_t wrap_sub(int16_t a, int16_t b) {
  return static_cast<int16_t>(static_cast<uint16_t>(a) - static_cast<uint16_t>(b));
}

// 16-bit wrapping multiply (LEGACY_S16_WRAP_MUL): 32-bit product truncated to
// the low 16 bits.
inline int16_t wrap_mul(int16_t a, int16_t b) {
  return static_cast<int16_t>(static_cast<uint16_t>(static_cast<int32_t>(a) * b));
}

inline int16_t wrap_negate(int16_t v) {
  return static_cast<int16_t>(static_cast<uint16_t>(0) - static_cast<uint16_t>(v));
}

// 32-bit wrapping add/subtract (LEGACY_S32_WRAP_ADD/SUB).
inline int32_t wrap_add32(int32_t a, int32_t b) {
  return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}

inline int32_t wrap_sub32(int32_t a, int32_t b) {
  return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

// Low 16 bits of a long, reinterpreted as signed (LEGACY_S16_FROM_BITS).
inline int16_t from_bits_u16(uint16_t v) { return static_cast<int16_t>(v); }

// (LEGACY_S16_SAR(a, count))((u16)a >> count) as an unsigned count-style value;
// equivalent to sar16().
inline int16_t absolute16(int16_t v) { return v < 0 ? wrap_negate(v) : v; }

// Signed division with the original zero guard (LEGACY_S16_DIV_OR_ZERO).
inline int16_t div_or_zero(int16_t a, int16_t b) {
  return b == 0 ? 0 : static_cast<int16_t>(a / b);
}

}  // namespace s3d::physics