#include <fstream>
#include <vector>

#include "assets/trk.h"
#include "core/paths.h"
#include "tiny_test.h"

using namespace s3d;

TEST(trk_rejects_short_buffer) {
  const uint8_t small[16] = {0};
  Track t = Track::parse(small, sizeof(small));
  EXPECT_EQ(t.sky, 0);
}

TEST(trk_parses_synthetic_track) {
  std::vector<uint8_t> buf(Track::kFileSize, 0);
  buf[0] = 0x04;                          // top-left item
  buf[Track::kCount] = 3;                 // skybox = city
  buf[Track::kCount + 1] = 0x06;          // first terrain byte (bottom row)
  buf[Track::kFileSize - 1] = 0xAB;       // unknown tail
  Track t = Track::parse(buf.data(), buf.size());
  EXPECT_EQ(t.item(0, 0), 0x04);
  EXPECT_EQ(t.sky, 3);
  // File terrain row 0 is the bottom row => our y = 29.
  EXPECT_EQ(t.terrain_at(0, Track::kSize - 1), 0x06);
  EXPECT_EQ(t.unknown_tail, 0xAB);
}

TEST(trk_loads_real_default_track) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  Track t = Track::load(path_join(dir, "DEFAULT.TRK"));
  // DEFAULT.TRK is exactly 1802 bytes with a known skybox byte.
  EXPECT_TRUE(t.sky <= 4);
  int non_blank = 0;
  for (size_t i = 0; i < Track::kCount; ++i) {
    if (t.items[i] != 0) ++non_blank;
  }
  EXPECT_TRUE(non_blank > 0);
}
