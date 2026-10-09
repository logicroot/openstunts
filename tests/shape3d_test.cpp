#include <string>

#include "assets/shape3d.h"
#include "core/paths.h"
#include "tiny_test.h"

using namespace s3d;

TEST(shape3d_track_name_order) {
  EXPECT_EQ(ShapeLibrary::track_shape_count(), 116u);
  EXPECT_EQ(std::string(ShapeLibrary::track_shape_name(0)), std::string("barn"));
  EXPECT_EQ(std::string(ShapeLibrary::track_shape_name(30)), std::string("road"));
  EXPECT_EQ(std::string(ShapeLibrary::track_shape_name(60)), std::string("bank"));
  EXPECT_EQ(std::string(ShapeLibrary::track_shape_name(115)), std::string("exp3"));
}

TEST(shape3d_loads_game1_road) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  EXPECT_TRUE(lib.load_p3s(path_join(dir, "GAME1.P3S")));
  EXPECT_TRUE(lib.shapes().size() >= 50u);

  const Shape3D* road = lib.find("road");
  EXPECT_TRUE(road != nullptr);
  if (road != nullptr) {
    EXPECT_TRUE(road->vertices.size() > 0);
    EXPECT_TRUE(road->primitives.size() > 0);
    // Every primitive must reference valid vertices.
    bool ok = true;
    for (const auto& p : road->primitives) {
      for (uint8_t idx : p.indices) {
        if (idx >= road->vertices.size()) ok = false;
      }
    }
    EXPECT_TRUE(ok);
  }
}

TEST(shape3d_game2_wins_missing_game1_shapes) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  EXPECT_TRUE(lib.load_p3s(path_join(dir, "GAME1.P3S")));
  EXPECT_TRUE(lib.load_p3s(path_join(dir, "GAME2.P3S")));

  // "barn" only exists in GAME2; "road" only in GAME1.
  EXPECT_TRUE(lib.find("barn") != nullptr);
  EXPECT_TRUE(lib.find("road") != nullptr);
  EXPECT_TRUE(lib.find("tree") != nullptr);
  EXPECT_TRUE(lib.find("does_not_exist") == nullptr);

  // GAME1 precedence: "zban" exists in both; GAME1's must win. Just check it
  // loads without error and is present.
  EXPECT_TRUE(lib.find("zban") != nullptr);
}
