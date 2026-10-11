// 2D sprite (.PVS 2D-shape archive) loader tests. The in-game direction arrows
// are the SDGAME2.PVS "left"/"righ" records; a 24x15 triangle with a black
// outline, grey edge, white fill and 0xFF clear background. Data lives in the
// Stunts asset directory, so the tests skip cleanly when it is absent.

#include <string>

#include "assets/sprite2d.h"
#include "core/paths.h"
#include "tiny_test.h"

using namespace s3d;

namespace {

bool load_arrows(Sprite2D* left, Sprite2D* right) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return false;
  }
  const std::string pvs = path_join(dir, "SDGAME2.PVS");
  if (!load_sprite2d(pvs, "left", left) || !load_sprite2d(pvs, "righ", right)) {
    std::printf("  (skipped: SDGAME2.PVS arrows not found)\n");
    return false;
  }
  return true;
}

}  // namespace

TEST(sprite2d_left_arrow_header_and_size) {
  Sprite2D left, right;
  if (!load_arrows(&left, &right)) return;
  EXPECT_TRUE(left.valid());
  EXPECT_EQ(left.width, 24);
  EXPECT_EQ(left.height, 15);
  // SHAPE2D.s2d_pos_x / s2d_pos_y as stored in SDGAME2.PVS.
  EXPECT_EQ(left.pos_x, 168);
  EXPECT_EQ(left.pos_y, 91);
  EXPECT_EQ(left.pixels.size(), 24u * 15u);
}

TEST(sprite2d_arrows_are_horizontal_mirrors) {
  Sprite2D left, right;
  if (!load_arrows(&left, &right)) return;
  EXPECT_EQ(right.width, left.width);
  EXPECT_EQ(right.height, left.height);
  for (int y = 0; y < left.height; ++y) {
    for (int x = 0; x < left.width; ++x) {
      const uint8_t l = left.pixels[static_cast<size_t>(y) * left.width + x];
      const uint8_t r =
          right.pixels[static_cast<size_t>(y) * right.width + (right.width - 1 - x)];
      EXPECT_EQ(l, r);
    }
  }
}

TEST(sprite2d_left_arrow_points_left) {
  Sprite2D left, right;
  if (!load_arrows(&left, &right)) return;
  auto at = [](const Sprite2D& s, int x, int y) {
    return s.pixels[static_cast<size_t>(y) * s.width + x];
  };
  // The tip sits at the left edge, mid height; the corners are clear (0xFF).
  EXPECT_TRUE(at(left, 0, 7) < 16);
  EXPECT_EQ(at(left, 0, 0), 0xFF);
  EXPECT_EQ(at(left, 23, 0), 0xFF);
  // The right arrow is the mirror: its tip is on the right edge.
  EXPECT_TRUE(at(right, 23, 7) < 16);
  EXPECT_EQ(at(right, 0, 0), 0xFF);
}

TEST(sprite2d_missing_asset_returns_false) {
  Sprite2D s;
  EXPECT_TRUE(!load_sprite2d("/nonexistent/SDGAME2.PVS", "left", &s));
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  EXPECT_TRUE(!load_sprite2d(path_join(dir, "SDGAME2.PVS"), "nope", &s));
}
