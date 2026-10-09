#pragma once
// VGA palette used by the original art. The 256-entry DAC table lives in
// `SDMAIN.PVS` as resource `!pal` (a 16-byte 2D-shape header followed by 768
// bytes of 256 RGB triplets with 6-bit components, 0..63).
//
// Reference: restunts docs/formats.md and `docs/game.res.txt`.

#include <cstdint>
#include <string>

namespace s3d {

struct Palette {
  // Raw 6-bit DAC components exactly as stored (0..63).
  uint8_t raw6[256][3] = {};
  // 8-bit expansion: (v << 2) | (v >> 4).
  uint8_t rgb8[256][3] = {};

  void set_from_dac(const uint8_t* dac768);
};

// Loads `!pal` from the given `.PVS`/`.PRE` archive (decompressing it), or from
// a `.RES` archive. Returns false if the archive cannot be read or lacks `!pal`.
bool load_vga_palette(const std::string& archive_path, Palette* out);

}  // namespace s3d
