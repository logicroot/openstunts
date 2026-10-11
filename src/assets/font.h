#pragma once
// Stunts bitmap font (.FNT) loader.
//
// Layout (restunts font_draw_text, seg012.asm; the same data the original
// font_set_fontdef2/font_draw_text pair consumes):
//
//   bytes 0x000..0x015   header (22 bytes)
//   bytes 0x016..0x215   256 little-endian u16 glyph offsets (0 = no glyph)
//   bytes 0x216..        glyph bitmaps
//
// Header fields:
//   +0x00 (u16) cursor x      (runtime scratch)
//   +0x02 (u16) cursor y      (runtime scratch)
//   +0x04 (u8)  bytes per glyph row
//   +0x06 (u16) glyph rows (pixel height)
//   +0x08 (u16) advance width for fixed-pitch fonts
//   +0x0A (u16) line height (newline advance)
//   +0x0C (u8)  proportional flag: the glyph's first byte is its width
//
// A glyph is `rows * bytes_per_row` bytes; each byte holds 8 pixels, MSB
// (leftmost) first. The original blitter always draws whole bytes and advances
// by the glyph width, so wide glyphs can overlap their trailing neighbour; the
// quad builder reproduces that by drawing `columns` pixels and advancing
// `advance`.

#include <cstdint>
#include <string>
#include <vector>

namespace s3d {

struct FontGlyph {
  int advance = 0;            // x pixels to move after drawing
  int columns = 0;           // bitmap columns (bytes_per_row * 8)
  std::vector<uint8_t> bits;  // rows * (columns / 8) bytes, MSB first
};

class BitmapFont {
 public:
  static constexpr int kGlyphCount = 256;

  bool load(const std::string& path);
  bool load(const uint8_t* data, size_t size);

  bool valid() const { return valid_; }
  int rows() const { return rows_; }
  int line_height() const { return line_height_; }
  bool proportional() const { return proportional_; }

  // Glyphs are never null; a character with no bitmap yields an empty glyph
  // (advance = fallback, columns = 0), matching the original's blank output.
  const FontGlyph& glyph(uint8_t ch) const { return glyphs_[ch]; }

  // Width in pixels of a single line of `text` (no newline handling).
  int text_width(const std::string& text) const;

 private:
  bool parse(const uint8_t* data, size_t size);

  bool valid_ = false;
  int rows_ = 0;
  int bytes_per_row_ = 0;
  int advance_ = 0;
  int line_height_ = 0;
  bool proportional_ = false;
  FontGlyph glyphs_[kGlyphCount];
};

}  // namespace s3d
