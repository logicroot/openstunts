// Crashed-windscreen overlay (seg003.asm init_crak) tests. GAME.PRE holds the
// "cinf" stage table and the "crak" segment list; a synthetic buffer exercises
// the decoder and the stage math, and the real asset is checked when present.

#include <cstdint>
#include <string>
#include <vector>

#include "assets/crash_overlay.h"
#include "core/paths.h"
#include "tiny_test.h"

using namespace s3d;

namespace {

std::vector<uint8_t> make_cinf(const std::vector<uint16_t>& counts) {
  std::vector<uint8_t> b;
  auto push16 = [&](uint16_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFF));
    b.push_back(static_cast<uint8_t>(v >> 8));
  };
  push16(static_cast<uint16_t>(counts.size()));
  for (uint16_t c : counts) push16(c);
  return b;
}

std::vector<uint8_t> make_crak(int n) {
  std::vector<uint8_t> b;
  for (int i = 0; i < n; ++i) {
    const uint16_t v[4] = {static_cast<uint16_t>(i), 2, 3, 4};
    for (uint16_t w : v) {
      b.push_back(static_cast<uint8_t>(w & 0xFF));
      b.push_back(static_cast<uint8_t>(w >> 8));
    }
  }
  return b;
}

}  // namespace

TEST(crash_overlay_decodes_synthetic_chunks) {
  CrashOverlay ov;
  EXPECT_TRUE(ov.decode(make_crak(77), make_cinf({6, 18, 42, 77})));
  EXPECT_TRUE(ov.valid());
  EXPECT_EQ(ov.stage_count(), 4);
  EXPECT_EQ(ov.segments().size(), 77u);
  EXPECT_EQ(ov.segments()[0].x1, 0);
  EXPECT_EQ(ov.segments()[0].y2, 4);
  EXPECT_EQ(ov.segments()[10].x1, 10);
}

TEST(crash_overlay_stage_progression) {
  CrashOverlay ov;
  EXPECT_TRUE(ov.decode(make_crak(77), make_cinf({6, 18, 42, 77})));
  if (!ov.valid()) return;
  // framecount / (framespersec / 7) with framespersec = 20 -> framecount / 2,
  // clamped to stage_count - 1 (statecrs/seg003 init_crak).
  EXPECT_EQ(ov.stage(0, 20), 0);
  EXPECT_EQ(ov.stage(1, 20), 0);
  EXPECT_EQ(ov.stage(2, 20), 1);
  EXPECT_EQ(ov.stage(4, 20), 2);
  EXPECT_EQ(ov.stage(6, 20), 3);
  EXPECT_EQ(ov.stage(1000, 20), 3);
  EXPECT_EQ(ov.visible_segments(0, 20), 6);
  EXPECT_EQ(ov.visible_segments(2, 20), 18);
  EXPECT_EQ(ov.visible_segments(4, 20), 42);
  EXPECT_EQ(ov.visible_segments(6, 20), 77);
  EXPECT_EQ(ov.visible_segments(1000, 20), 77);
}

TEST(crash_overlay_rejects_malformed_chunks) {
  CrashOverlay ov;
  EXPECT_TRUE(!ov.decode(make_crak(4), {}));              // no cinf
  const std::vector<uint8_t> bad_cinf = {2, 0, 5, 0};     // stage_count 2, one count
  EXPECT_TRUE(!ov.decode(make_crak(4), bad_cinf));        // truncated cinf
  EXPECT_TRUE(!ov.decode({}, make_cinf({4})));            // no crak
  const std::vector<uint8_t> seven(7, 0);
  EXPECT_TRUE(!ov.decode(seven, make_cinf({4})));         // crak not a multiple of 8
}

TEST(crash_overlay_real_game_pre) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  CrashOverlay ov;
  if (!ov.load_from_game_pre(path_join(dir, "GAME.PRE"))) {
    std::printf("  (skipped: GAME.PRE crak/cinf not found)\n");
    return;
  }
  EXPECT_TRUE(ov.valid());
  EXPECT_EQ(ov.stage_count(), 4);
  EXPECT_EQ(ov.segments().size(), 77u);
  EXPECT_EQ(ov.visible_segments(0, 20), 6);
  EXPECT_EQ(ov.visible_segments(6, 20), 77);
  // First crack segment as stored: (131,84) -> (159,66).
  EXPECT_EQ(ov.segments()[0].x1, 131);
  EXPECT_EQ(ov.segments()[0].y1, 84);
  EXPECT_EQ(ov.segments()[0].x2, 159);
  EXPECT_EQ(ov.segments()[0].y2, 66);
}
