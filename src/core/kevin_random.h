#pragma once
// The original engine's `kevinrandom` generator (restunts restunts.c
// `init_kevinrandom` / `get_kevinrandom`, seeded with "kevin").
//
// It is a 6-byte additive (Fibonacci-style) generator with a 6-byte counter:
//
//   seed[4] += seed[5]; seed[3] += seed[4]; seed[2] += seed[3];
//   seed[1] += seed[2]; seed[0] += seed[1];
//   if (!++seed[5] && !++seed[4] && !++seed[3] && !++seed[2] && !++seed[1])
//       ++seed[0];
//   return seed[0];
//
// `next()` returns one byte (0..255); the original's callers widen it to a
// 16-bit value. The original consumes randoms for many things (the race
// line-up, `run_game_random`, the crash debris, ...); the port only uses them
// for the crash debris, so the values will not match a recorded original run
// tick-for-tick, but they are repeatable within a run.

#include <cstdint>

namespace s3d {

class KevinRandom {
 public:
  KevinRandom() = default;
  explicit KevinRandom(const char* seed) { reset(seed); }

  // Re-seeds from the first 6 bytes at `seed`. The original is called with the
  // 5-character literal "kevin", so the 6th seed byte is its NUL terminator
  // (a `const char*` to a string literal is therefore a valid argument).
  void reset(const char* seed) {
    for (int i = 0; i < kSeedLength; ++i) {
      seed_[i] = static_cast<uint8_t>(seed[i]);
    }
  }

  // Advances the generator and returns the next byte (0..255).
  uint8_t next() {
    seed_[4] = static_cast<uint8_t>(seed_[4] + seed_[5]);
    seed_[3] = static_cast<uint8_t>(seed_[3] + seed_[4]);
    seed_[2] = static_cast<uint8_t>(seed_[2] + seed_[3]);
    seed_[1] = static_cast<uint8_t>(seed_[1] + seed_[2]);
    seed_[0] = static_cast<uint8_t>(seed_[0] + seed_[1]);
    // Carry chain: ++seed[5], and on wrap ++seed[4], and so on to seed[0].
    if (++seed_[5] == 0 && ++seed_[4] == 0 && ++seed_[3] == 0 && ++seed_[2] == 0 &&
        ++seed_[1] == 0) {
      ++seed_[0];
    }
    return seed_[0];
  }

  const uint8_t* seed() const { return seed_; }

 private:
  static constexpr int kSeedLength = 6;
  uint8_t seed_[kSeedLength] = {0, 0, 0, 0, 0, 0};
};

}  // namespace s3d
