// Skybox (horizon panorama) tests. Covers the TRK theme -> archive name map,
// the skybox_layout panorama placement maths (pure, no assets) and, when the
// Stunts asset directory is present, the four scenery layers of each theme
// (seg003.asm load_skybox / skybox_op_helper2).

#include <algorithm>
#include <string>
#include <vector>

#include "assets/trk.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/skybox.h"

using namespace s3d;

namespace {

// Builds a synthetic skybox with the real layer dimensions but dummy pixels, so
// skybox_layout can be exercised without the game assets.
world::Skybox synthetic_skybox() {
  world::Skybox sky;
  const int w[4] = {320, 192, 320, 192};
  const int h[4] = {13, 19, 10, 24};
  for (int i = 0; i < world::kSkyboxLayerCount; ++i) {
    world::SkyboxLayer& l = sky.layers[static_cast<size_t>(i)];
    l.sprite.width = w[i];
    l.sprite.height = h[i];
    l.sprite.pixels.assign(static_cast<size_t>(w[i]) * static_cast<size_t>(h[i]), 0);
    l.offset_x = world::kSkyboxLayerOffsets[i];
  }
  sky.loaded = true;
  return sky;
}

}  // namespace

TEST(skybox_theme_names) {
  Track t;
  t.sky = 0;
  EXPECT_EQ(std::string(t.skybox_name()), std::string("desert"));
  t.sky = 1;
  EXPECT_EQ(std::string(t.skybox_name()), std::string("tropical"));
  t.sky = 2;
  EXPECT_EQ(std::string(t.skybox_name()), std::string("alpine"));
  t.sky = 3;
  EXPECT_EQ(std::string(t.skybox_name()), std::string("city"));
  t.sky = 4;
  EXPECT_EQ(std::string(t.skybox_name()), std::string("country"));
  t.sky = 200;
  EXPECT_EQ(std::string(t.skybox_name()), std::string("unknown"));
}

TEST(skybox_layer_metadata) {
  EXPECT_EQ(world::kSkyboxLayerOffsets[0], 0);
  EXPECT_EQ(world::kSkyboxLayerOffsets[1], 320);
  EXPECT_EQ(world::kSkyboxLayerOffsets[2], 512);
  EXPECT_EQ(world::kSkyboxLayerOffsets[3], 832);
  EXPECT_EQ(std::string(world::kSkyboxLayerIds[0]), std::string("scen"));
  EXPECT_EQ(std::string(world::kSkyboxLayerIds[1]), std::string("sce2"));
  EXPECT_EQ(std::string(world::kSkyboxLayerIds[2]), std::string("sce3"));
  EXPECT_EQ(std::string(world::kSkyboxLayerIds[3]), std::string("sce4"));
}

TEST(skybox_min_max_height) {
  const world::Skybox sky = synthetic_skybox();
  EXPECT_EQ(sky.min_height(), 10);  // sce3 is the shortest layer
  EXPECT_EQ(sky.max_height(), 24);  // sce4 is the tallest
}

TEST(skybox_layout_centered) {
  const world::Skybox sky = synthetic_skybox();
  world::SkyboxPlacement p[16];
  // yaw = 0 -> panorama column 0x200 shows through the 320-wide window, which
  // is entirely inside the sce3 layer [512, 832).
  const int n = world::skybox_layout(sky, 320, 200, 100.0f, 0, p, 16);
  EXPECT_EQ(n, 1);
  if (n == 1) {
    EXPECT_EQ(p[0].layer, 2);
    EXPECT_EQ(p[0].x, 0.0f);
    EXPECT_EQ(p[0].width, 320.0f);
    EXPECT_EQ(p[0].height, 10.0f);
    EXPECT_EQ(p[0].y, 90.0f);  // horizon - height
  }
}

TEST(skybox_layout_quarter_turn) {
  const world::Skybox sky = synthetic_skybox();
  world::SkyboxPlacement p[16];
  // yaw = 0x200 -> panoramic x = 0, the scen layer starts exactly at the left.
  const int n = world::skybox_layout(sky, 320, 200, 100.0f, 0x200, p, 16);
  EXPECT_EQ(n, 1);
  if (n == 1) {
    EXPECT_EQ(p[0].layer, 0);
    EXPECT_EQ(p[0].x, 0.0f);
    EXPECT_EQ(p[0].width, 320.0f);
    EXPECT_EQ(p[0].height, 13.0f);
    EXPECT_EQ(p[0].y, 87.0f);
  }
}

TEST(skybox_layout_straddles_layer_seam) {
  const world::Skybox sky = synthetic_skybox();
  world::SkyboxPlacement p[16];
  // yaw = 0x40 scrolls 64 pixels on, so the window shows the tail of sce2 and
  // the start of sce3 with a seam at screen x = 64.
  const int n = world::skybox_layout(sky, 320, 200, 100.0f, 0x40, p, 16);
  EXPECT_EQ(n, 2);
  if (n == 2) {
    EXPECT_EQ(p[0].layer, 1);
    EXPECT_EQ(p[0].x, -128.0f);
    EXPECT_EQ(p[0].width, 192.0f);
    EXPECT_EQ(p[0].y, 81.0f);  // 100 - 19
    EXPECT_EQ(p[1].layer, 2);
    EXPECT_EQ(p[1].x, 64.0f);
    EXPECT_EQ(p[1].width, 320.0f);
    EXPECT_EQ(p[1].y, 90.0f);  // 100 - 10
  }
}

TEST(skybox_layout_scales_with_view_height) {
  const world::Skybox sky = synthetic_skybox();
  world::SkyboxPlacement p[16];
  const int n = world::skybox_layout(sky, 640, 400, 200.0f, 0, p, 16);
  EXPECT_EQ(n, 1);
  if (n == 1) {
    EXPECT_EQ(p[0].layer, 2);
    EXPECT_EQ(p[0].x, 0.0f);
    EXPECT_EQ(p[0].width, 640.0f);
    EXPECT_EQ(p[0].height, 20.0f);  // 10 * 400/200
    EXPECT_EQ(p[0].y, 180.0f);
  }
}

TEST(skybox_layout_covers_every_column) {
  const world::Skybox sky = synthetic_skybox();
  // For a sweep of yaw values the union of the (clipped) layer rectangles must
  // cover the whole 320-pixel window with no gaps.
  for (int yaw = -1024; yaw < 1024; yaw += 37) {
    world::SkyboxPlacement p[64];
    const int n = world::skybox_layout(sky, 320, 200, 100.0f, yaw, p, 64);
    std::vector<int> covered(320, 0);
    for (int i = 0; i < n; ++i) {
      const int x0 = std::max(0, static_cast<int>(p[i].x));
      const int x1 = std::min(320, static_cast<int>(p[i].x + p[i].width));
      for (int x = x0; x < x1; ++x) ++covered[static_cast<size_t>(x)];
    }
    for (int x = 0; x < 320; ++x) {
      if (covered[static_cast<size_t>(x)] == 0) {
        std::printf("  gap at yaw=%d x=%d\n", yaw, x);
        ++::s3d_test::failure_count();
        break;
      }
    }
  }
}

TEST(skybox_layout_ignores_invalid) {
  world::Skybox sky;  // not loaded
  world::SkyboxPlacement p[16];
  EXPECT_EQ(world::skybox_layout(sky, 320, 200, 100.0f, 0, p, 16), 0);
}

TEST(skybox_load_missing_theme) {
  world::Skybox sky;
  EXPECT_TRUE(!world::load_skybox("", "desert", &sky));
  EXPECT_TRUE(!sky.valid());
  EXPECT_TRUE(!world::load_skybox("does/not/exist", "desert", &sky));
}

TEST(skybox_load_theme_layers) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }

  struct Case {
    uint8_t sky;
    int min_height;
    int max_height;
  };
  const Case cases[] = {
      {0, 10, 24},  // desert: 13, 19, 10, 24
      {1, 11, 29},  // tropical: 11, 11, 19, 29
      {2, 14, 30},  // alpine: 14, 26, 16, 30
  };
  int loaded = 0;
  for (const Case& c : cases) {
    Track t;
    t.sky = c.sky;
    world::Skybox sky;
    if (!world::load_skybox(dir, t.skybox_name(), &sky)) {
      std::printf("  (skipped %s: archive not found)\n", t.skybox_name());
      continue;
    }
    ++loaded;
    EXPECT_TRUE(sky.valid());
    const int expected_widths[4] = {320, 192, 320, 192};
    int sum = 0;
    for (int i = 0; i < world::kSkyboxLayerCount; ++i) {
      const world::SkyboxLayer& l = sky.layers[static_cast<size_t>(i)];
      EXPECT_TRUE(l.valid());
      EXPECT_EQ(l.sprite.width, expected_widths[i]);
      EXPECT_EQ(l.offset_x, world::kSkyboxLayerOffsets[i]);
      sum += l.sprite.width;
    }
    EXPECT_EQ(sum, world::kSkyboxPanoramaWidth);
    EXPECT_EQ(sky.min_height(), c.min_height);
    EXPECT_EQ(sky.max_height(), c.max_height);
  }
  if (loaded == 0) std::printf("  (skipped: no skybox theme archives found)\n");
}
