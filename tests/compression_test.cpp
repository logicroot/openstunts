#include <string>

#include "assets/compression.h"
#include "assets/res_file.h"
#include "core/paths.h"
#include "tiny_test.h"

using namespace s3d;

TEST(decompress_game_pre_yields_plan_and_wall) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  std::vector<uint8_t> raw;
  EXPECT_TRUE(decompress_file(path_join(dir, "GAME.PRE"), &raw));
  EXPECT_EQ(raw.size(), 20890u);

  ResFile res;
  EXPECT_TRUE(res.load(raw.data(), raw.size()));
  EXPECT_EQ(res.chunks().size(), 20u);

  const ResFile::Chunk* plan = res.find("plan");
  EXPECT_TRUE(plan != nullptr);
  EXPECT_EQ(plan->size, 536u * 34u);  // 18224

  const ResFile::Chunk* wall = res.find("wall");
  EXPECT_TRUE(wall != nullptr);
  EXPECT_EQ(wall->size, 191u * 6u);  // 1146
}

TEST(decompress_first_plane_matches_reference) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  std::vector<uint8_t> raw;
  EXPECT_TRUE(decompress_file(path_join(dir, "GAME.PRE"), &raw));
  ResFile res;
  EXPECT_TRUE(res.load(raw.data(), raw.size()));
  const std::vector<uint8_t> plan = res.read("plan");
  if (plan.size() < 34) {
    EXPECT_TRUE(plan.size() >= 34);
    return;
  }

  auto s16 = [&](size_t off) {
    return static_cast<int16_t>(plan[off] | (plan[off + 1] << 8));
  };
  EXPECT_EQ(s16(0), 0);      // angleYZ
  EXPECT_EQ(s16(2), 0);      // angleXY
  EXPECT_EQ(s16(4), 0);      // origin.x
  EXPECT_EQ(s16(6), 0);      // origin.y
  EXPECT_EQ(s16(8), 0);      // origin.z
  EXPECT_EQ(s16(10), 0);     // normal.x
  EXPECT_EQ(s16(12), 8192);  // normal.y (length 0x2000)
  EXPECT_EQ(s16(14), 0);     // normal.z
}

TEST(decompress_sdmain_pvs_palette) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  std::vector<uint8_t> raw;
  EXPECT_TRUE(decompress_file(path_join(dir, "SDMAIN.PVS"), &raw));
  EXPECT_EQ(raw.size(), 1732u);

  ResFile res;
  EXPECT_TRUE(res.load(raw.data(), raw.size()));
  EXPECT_EQ(res.chunks().size(), 5u);

  const std::vector<uint8_t> pal = res.read("!pal");
  EXPECT_EQ(pal.size(), 784u);  // 16-byte SHAPE2D header + 768 RGB
  // First palette entry is black, second is (0,0,42) in 6-bit components.
  EXPECT_EQ(pal[16 + 0], 0);
  EXPECT_EQ(pal[16 + 1], 0);
  EXPECT_EQ(pal[16 + 2], 0);
  EXPECT_EQ(pal[16 + 3], 0);
  EXPECT_EQ(pal[16 + 4], 0);
  EXPECT_EQ(pal[16 + 5], 42u);

  EXPECT_EQ(res.read("!eg0").size(), 272u);
  EXPECT_EQ(res.read("mmou").size(), 176u);
}

TEST(decompress_two_pass_shape_archive) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  std::vector<uint8_t> raw;
  EXPECT_TRUE(decompress_file(path_join(dir, "GAME1.P3S"), &raw));
  EXPECT_EQ(raw.size(), 29078u);

  ResFile res;
  EXPECT_TRUE(res.load(raw.data(), raw.size()));
  EXPECT_TRUE(res.chunks().size() > 20u);
}
