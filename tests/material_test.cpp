#include <string>

#include "assets/palette.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/material.h"

using namespace s3d;
using namespace s3d::world;

// The table was ported verbatim from restunts dseg.asm material_color_list. A
// few spot checks cover the low base colours, the pavement/ground/sky/water
// entries the engine reads by fixed index, and the last (128) entry.
TEST(material_color_index_matches_reference) {
  EXPECT_EQ(material_color_index(0), 0u);
  EXPECT_EQ(material_color_index(1), 1u);
  EXPECT_EQ(material_color_index(15), 15u);
  EXPECT_EQ(material_color_index(16), 108u);  // ground colour (0x10)
  EXPECT_EQ(material_color_index(17), 116u);  // sky colour (0x11)
  EXPECT_EQ(material_color_index(19), 28u);   // asphalt
  EXPECT_EQ(material_color_index(21), 14u);   // road marking (yellow)
  EXPECT_EQ(material_color_index(100), 136u); // water (0x64)
  EXPECT_EQ(material_color_index(128), 17u);  // last entry
}

// The rear lights override the "backlight" livery paint job (0x2D) with
// material 0x2E when coasting and 0x2F while braking (frame.c). Both resolve to
// palette reds, the braking one brighter.
TEST(material_color_index_backlights) {
  EXPECT_EQ(material_color_index(0x2D), 4u);
  EXPECT_EQ(material_color_index(0x2E), 4u);
  EXPECT_EQ(material_color_index(0x2F), 12u);
}

TEST(material_color_index_out_of_range_is_black) {
  EXPECT_EQ(material_color_index(kMaterialCount), 0u);
  EXPECT_EQ(material_color_index(200), 0u);
  EXPECT_EQ(material_color_index(255), 0u);
}

TEST(material_patterned_matches_reference) {
  // material_pattern_list is non-zero at 22, 23, 24, 34, 94 and 118..125.
  EXPECT_TRUE(material_patterned(22));
  EXPECT_TRUE(material_patterned(23));
  EXPECT_TRUE(material_patterned(24));
  EXPECT_TRUE(material_patterned(34));
  EXPECT_TRUE(material_patterned(94));
  for (int i = 118; i <= 125; ++i) EXPECT_TRUE(material_patterned(i));
  EXPECT_TRUE(!material_patterned(0));
  EXPECT_TRUE(!material_patterned(19));
  EXPECT_TRUE(!material_patterned(21));
}

TEST(material_transparent_matches_reference) {
  // Material 118 is the "windmill animation mask": the original lists it as
  // pattern "Transparent" with no palette entry, so it must draw nothing.
  EXPECT_TRUE(material_transparent(118));
  EXPECT_TRUE(!material_transparent(113));  // windmill sail
  EXPECT_TRUE(!material_transparent(0));
  EXPECT_TRUE(!material_transparent(19));
}

TEST(material_pattern_masks_match_reference) {
  // material_pattern2_list (dseg.asm 21364..21492): the 8x2 dither tile used by
  // draw_patterned_lines. Check the values the road and windmill materials use.
  EXPECT_EQ(material_pattern(22), 0x77DDu);  // 30685
  EXPECT_EQ(material_pattern(23), 0xDD77u);  // 56695
  EXPECT_EQ(material_pattern(24), 0x77DDu);
  EXPECT_EQ(material_pattern(34), 0x77DDu);
  EXPECT_EQ(material_pattern(94), 0x77DDu);
  EXPECT_EQ(material_pattern(119), 0xCC33u);
  EXPECT_EQ(material_pattern(120), 0x33CCu);
  EXPECT_EQ(material_pattern(121), 0xB66Du);
  EXPECT_EQ(material_pattern(122), 0x4992u);
  EXPECT_EQ(material_pattern(123), 0xB66Du);
  EXPECT_EQ(material_pattern(124), 0x4992u);
  EXPECT_EQ(material_pattern(125), 0xB66Du);
  EXPECT_EQ(material_pattern(126), 0x4992u);
}

TEST(material_pattern_flat_and_transparent_are_zero) {
  // Flat materials have no mask.
  EXPECT_EQ(material_pattern(0), 0u);
  EXPECT_EQ(material_pattern(19), 0u);
  EXPECT_EQ(material_pattern(21), 0u);
  // Material 118 is the "pattern transparent" windmill mask: patterned, but its
  // mask is 0, so it draws nothing.
  EXPECT_EQ(material_pattern(118), 0u);
  // Out of range is flat.
  EXPECT_EQ(material_pattern(kMaterialCount), 0u);
  EXPECT_EQ(material_pattern(255), 0u);
}

TEST(material_color_samples_vga_palette) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  Palette pal;
  if (!load_vga_palette(path_join(dir, "SDMAIN.PVS"), &pal)) {
    std::printf("  (skipped: no VGA palette found)\n");
    return;
  }

  // Ground material 16 -> palette 108 (green); asphalt 19 -> palette 28 (grey).
  const glm::vec4 ground = material_color(16, pal);
  EXPECT_TRUE(ground.g > ground.r);
  EXPECT_TRUE(ground.g > ground.b);

  const glm::vec4 asphalt = material_color(19, pal);
  EXPECT_EQ(asphalt.r, asphalt.g);
  EXPECT_EQ(asphalt.g, asphalt.b);
  EXPECT_EQ(asphalt.a, 1.0f);
}
