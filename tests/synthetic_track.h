#pragma once
// A minimal closed route shared by the race/route tests: an 8-cell ring around
// the (col 2, bottom-up row 2) cell, made of the start/finish straight (element
// 1), plain road straights (4 = north/south, 5 = east/west) and the four classic
// corners (7 = north->west, 6 = west->south, 8 = south->east, 9 = east->north).
//
// The `.TRK` item grid is stored top-down, so bottom-up row `trow` lives at
// file row `29 - trow`. Cell centres are index * 1024 + 512 world units.

#include <cstdint>

#include "assets/trk.h"

namespace s3d_test {

inline s3d::Track make_loop_track() {
  s3d::Track t;
  const auto put = [&](int col, int row, uint8_t elem) {
    t.items[static_cast<size_t>(row) * s3d::Track::kSize + col] = elem;
  };
  put(3, 27, 0x01);  // trow 2: start/finish straight, angle 0
  put(3, 28, 0x07);  // trow 1: north -> west corner
  put(2, 28, 0x05);  // west straight
  put(1, 28, 0x06);  // west -> south corner
  put(1, 27, 0x04);  // south straight
  put(1, 26, 0x08);  // south -> east corner
  put(2, 26, 0x05);  // east straight
  put(3, 26, 0x09);  // east -> north corner
  return t;
}

inline int loop_center(int index) { return index * 1024 + 512; }

}  // namespace s3d_test
