#include "assets/sprite2d.h"

#include <utility>

#include "assets/compression.h"
#include "assets/res_file.h"

namespace s3d {

namespace {

uint16_t u16(const uint8_t* d, size_t o) {
  return static_cast<uint16_t>(d[o] | (static_cast<uint16_t>(d[o + 1]) << 8));
}

// SHAPE2D: width, height, unk1, unk2, pos_x, pos_y (6 words) + 4 flag bytes.
constexpr size_t kShape2DHeaderSize = 16;

}  // namespace

bool load_sprite2d(const std::string& pvs_path, const std::string& id,
                   Sprite2D* out) {
  if (out == nullptr) return false;
  std::vector<uint8_t> raw;
  if (!decompress_file(pvs_path, &raw)) return false;
  ResFile res;
  if (!res.load(raw.data(), raw.size())) return false;
  const std::vector<uint8_t> chunk = res.read(id);
  if (chunk.size() < kShape2DHeaderSize) return false;

  Sprite2D s;
  s.width = u16(chunk.data(), 0);
  s.height = u16(chunk.data(), 2);
  s.pos_x = u16(chunk.data(), 8);
  s.pos_y = u16(chunk.data(), 10);
  const size_t n = static_cast<size_t>(s.width) * static_cast<size_t>(s.height);
  if (s.width <= 0 || s.height <= 0 ||
      chunk.size() < kShape2DHeaderSize + n) {
    return false;
  }
  s.pixels.assign(chunk.begin() + kShape2DHeaderSize,
                  chunk.begin() + kShape2DHeaderSize + n);
  *out = std::move(s);
  return true;
}

}  // namespace s3d
