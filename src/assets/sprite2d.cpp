#include "assets/sprite2d.h"

#include <algorithm>
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

void unflip_shape2d(uint8_t* px, int w, int h, uint8_t unk5, uint8_t unk6) {
  if (px == nullptr || w <= 0 || h <= 0) return;
  // file_unflip_shape2d: the type is only read when s2d_unk6's high nibble is 0.
  if ((unk6 & 0xF0) != 0) return;
  const int type = unk5 >> 4;
  if (type == 0 || type > 3) return;  // 0 = not flipped; >3 is a fatal error in the game

  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  std::vector<uint8_t> tmp(n);
  if (type == 1) {
    // fliptype0: plain transpose, out[x][y] = in[y][x].
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        tmp[static_cast<size_t>(y) * w + x] = px[static_cast<size_t>(x) * h + y];
      }
    }
  } else if (type == 2) {
    // fliptype1: transpose with each stored column split into two interleaved
    // halves (even output rows take stored rows 0..h/2-1, odd rows the rest).
    for (int y = 0; y < h; y += 2) {
      const int rs = y / 2;
      for (int x = 0; x < w; ++x) {
        tmp[static_cast<size_t>(y) * w + x] = px[static_cast<size_t>(rs) + static_cast<size_t>(x) * h];
      }
    }
    for (int y = 1; y < h; y += 2) {
      const int rs = (y + h) >> 1;
      for (int x = 0; x < w; ++x) {
        tmp[static_cast<size_t>(y) * w + x] = px[static_cast<size_t>(rs) + static_cast<size_t>(x) * h];
      }
    }
  } else {  // type == 3
    // fliptype2: alternating (h+1)/2 and h/2 column strides.
    int bx = 0;
    int y = 0;
    while (y < h) {
      int s = (h + 1) / 2;
      for (int x = 0; x < w; ++x) {
        tmp[static_cast<size_t>(y) * w + x] =
            (bx >= 0 && static_cast<size_t>(bx) < n) ? px[bx] : 0;
        bx += s;
      }
      ++y;
      if (y >= h) break;
      s = h / 2;
      for (int x = 0; x < w; ++x) {
        tmp[static_cast<size_t>(y) * w + x] =
            (bx >= 0 && static_cast<size_t>(bx) < n) ? px[bx] : 0;
        bx += s;
      }
      ++y;
    }
  }
  std::copy(tmp.begin(), tmp.end(), px);
}

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
  s.pivot_x = u16(chunk.data(), 4);
  s.pivot_y = u16(chunk.data(), 6);
  s.pos_x = u16(chunk.data(), 8);
  s.pos_y = u16(chunk.data(), 10);
  const size_t n = static_cast<size_t>(s.width) * static_cast<size_t>(s.height);
  if (s.width <= 0 || s.height <= 0 ||
      chunk.size() < kShape2DHeaderSize + n) {
    return false;
  }
  s.pixels.assign(chunk.begin() + kShape2DHeaderSize,
                  chunk.begin() + kShape2DHeaderSize + n);
  // The game transposes the stored payload of a .PVS shape when the shape's
  // flip flag is set (seg012.asm file_unflip_shape2d); do it here so every caller
  // sees the same upright bitmap the original blits.
  unflip_shape2d(s.pixels.data(), s.width, s.height, chunk[14], chunk[15]);
  *out = std::move(s);
  return true;
}

}  // namespace s3d
