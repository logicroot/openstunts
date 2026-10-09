#pragma once
// Stunts track (.TRK) parser.
//
// Layout (see reference/restunts/docs/format.html):
//   bytes    0..899   900 item bytes, left->right, top->bottom
//   byte     900      skybox id (0=desert 1=tropical 2=alpine 3=city 4=country)
//   bytes  901..1800 900 terrain bytes, left->right, bottom->top
//   byte    1801      unknown
//
// This class normalizes both grids to a common convention: index = y*30 + x
// with x growing right and y growing down (y=0 is the top row).

#include <array>
#include <cstdint>
#include <string>

namespace s3d {

struct Track {
  static constexpr int kSize = 30;
  static constexpr int kCount = kSize * kSize;
  static constexpr size_t kFileSize = 1802;

  std::array<uint8_t, kCount> items{};
  std::array<uint8_t, kCount> terrain{};
  uint8_t sky = 0;
  uint8_t unknown_tail = 0;

  static Track parse(const uint8_t* data, size_t size);
  static Track load(const std::string& path);

  // x: column (0=left), y: row (0=top).
  uint8_t item(int x, int y) const { return items[static_cast<size_t>(y) * kSize + x]; }
  uint8_t terrain_at(int x, int y) const { return terrain[static_cast<size_t>(y) * kSize + x]; }

  const char* skybox_name() const;
};

}  // namespace s3d
