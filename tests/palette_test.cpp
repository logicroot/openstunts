#include <string>

#include "assets/palette.h"
#include "core/paths.h"
#include "tiny_test.h"

using namespace s3d;

TEST(palette_loads_sdmain) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  Palette pal;
  EXPECT_TRUE(load_vga_palette(path_join(dir, "SDMAIN.PVS"), &pal));

  // Entry 0 is black, entry 1 is (0,0,42) in 6-bit components.
  EXPECT_EQ(pal.raw6[0][0], 0u);
  EXPECT_EQ(pal.raw6[0][1], 0u);
  EXPECT_EQ(pal.raw6[0][2], 0u);
  EXPECT_EQ(pal.raw6[1][0], 0u);
  EXPECT_EQ(pal.raw6[1][1], 0u);
  EXPECT_EQ(pal.raw6[1][2], 42u);

  // 8-bit expansion of 42 is (42 << 2) | (42 >> 4) = 168 | 2 = 170.
  EXPECT_EQ(pal.rgb8[1][2], 170u);
}

TEST(palette_rejects_missing_archive) {
  Palette pal;
  EXPECT_TRUE(!load_vga_palette("/nonexistent/does_not_exist.pvs", &pal));
}
