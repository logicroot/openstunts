#include "assets/font.h"

#include <cstdio>
#include <fstream>
#include <iterator>

#include "core/log.h"

namespace s3d {

namespace {

uint8_t u8(const uint8_t* d, size_t o) { return d[o]; }
uint16_t u16(const uint8_t* d, size_t o) {
  return static_cast<uint16_t>(d[o] | (static_cast<uint16_t>(d[o + 1]) << 8));
}

constexpr size_t kHeaderSize = 0x16;
constexpr size_t kTableSize = 256 * 2;

}  // namespace

bool BitmapFont::load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    S3D_ERROR("FNT: cannot open '%s'", path.c_str());
    return false;
  }
  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
  return parse(buf.data(), buf.size());
}

bool BitmapFont::load(const uint8_t* data, size_t size) { return parse(data, size); }

bool BitmapFont::parse(const uint8_t* d, size_t size) {
  valid_ = false;
  if (d == nullptr || size < kHeaderSize + kTableSize) {
    S3D_ERROR("FNT: file too small (%zu bytes)", size);
    return false;
  }

  bytes_per_row_ = u8(d, 0x0C);
  rows_ = u16(d, 0x0E);
  advance_ = u16(d, 0x10);
  line_height_ = u16(d, 0x12);
  proportional_ = u8(d, 0x14) != 0;
  if (rows_ <= 0 || bytes_per_row_ <= 0) {
    S3D_ERROR("FNT: bad header (rows=%d bytes/row=%d)", rows_, bytes_per_row_);
    return false;
  }

  for (int c = 0; c < kGlyphCount; ++c) {
    FontGlyph g;
    const uint16_t off = u16(d, kHeaderSize + static_cast<size_t>(c) * 2);
    if (off == 0) {
      // No bitmap: the original draws nothing and does not advance the cursor.
      glyphs_[c] = g;
      continue;
    }
    size_t p = off;
    int glyph_advance;
    int bpr;
    if (proportional_) {
      if (p >= size) return false;
      glyph_advance = d[p++];
      bpr = (glyph_advance + 7) / 8;
    } else {
      glyph_advance = advance_ > 0 ? advance_ : bytes_per_row_ * 8;
      bpr = bytes_per_row_;
    }
    if (bpr <= 0) bpr = 1;
    const size_t n = static_cast<size_t>(rows_) * static_cast<size_t>(bpr);
    if (p + n > size) {
      S3D_ERROR("FNT: glyph %d out of range (off=%u)", c, off);
      return false;
    }
    g.advance = glyph_advance;
    g.columns = bpr * 8;
    g.bits.assign(d + p, d + p + n);
    glyphs_[c] = std::move(g);
  }

  valid_ = true;
  return true;
}

int BitmapFont::text_width(const std::string& text) const {
  int w = 0;
  for (unsigned char ch : text) w += glyphs_[ch].advance;
  return w;
}

}  // namespace s3d
