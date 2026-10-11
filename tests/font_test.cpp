// Bitmap font (.FNT) loader tests. The LED font used by the race clock is a
// fixed-pitch 8-row font; FONTDEF is proportional. Data lives in the Stunts
// asset directory, so the tests skip cleanly when it is absent.

#include <string>

#include "assets/font.h"
#include "core/paths.h"
#include "tiny_test.h"

using namespace s3d;

namespace {

bool load_led(BitmapFont* font) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return false;
  }
  if (!font->load(path_join(dir, "FONTLED.FNT"))) {
    std::printf("  (skipped: FONTLED.FNT not found)\n");
    return false;
  }
  return true;
}

}  // namespace

TEST(font_led_metrics_match_reference_header) {
  BitmapFont font;
  if (!load_led(&font)) return;
  EXPECT_TRUE(font.valid());
  EXPECT_EQ(font.rows(), 8);
  EXPECT_EQ(font.line_height(), 8);
  EXPECT_TRUE(!font.proportional());  // fixed 6px advance
  EXPECT_EQ(font.glyph(' ').advance, 6);
  EXPECT_EQ(font.glyph('0').advance, 6);
  EXPECT_EQ(font.text_width("0:00"), 24);
}

TEST(font_led_digit_bitmaps_match_file) {
  BitmapFont font;
  if (!load_led(&font)) return;
  // From FONTLED.FNT: '0' is a 6x7 seven-segment shape in an 8-wide cell.
  const auto& zero = font.glyph('0');
  const uint8_t expected[8] = {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00};
  EXPECT_EQ(zero.bits.size(), 8u);
  for (int i = 0; i < 8; ++i) EXPECT_EQ(zero.bits[static_cast<size_t>(i)], expected[i]);
  // Undefined glyphs draw nothing and do not advance the cursor.
  EXPECT_EQ(font.glyph(0x01).bits.size(), 0u);
  EXPECT_EQ(font.glyph(0x01).advance, 0);
}

TEST(font_fontdef_is_proportional) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  BitmapFont font;
  if (!font.load(path_join(dir, "FONTDEF.FNT"))) {
    std::printf("  (skipped: FONTDEF.FNT not found)\n");
    return;
  }
  EXPECT_TRUE(font.valid());
  EXPECT_EQ(font.rows(), 8);
  EXPECT_TRUE(font.proportional());
  // Proportional fonts carry the width in the glyph's first byte.
  EXPECT_TRUE(font.glyph('A').advance > 0);
  EXPECT_TRUE(font.glyph('i').advance > 0);
  EXPECT_TRUE(font.glyph('W').advance > font.glyph('i').advance);
}
