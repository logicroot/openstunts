#include <glm/glm.hpp>

#include <string>

#include "assets/shape3d.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"

using namespace s3d;

TEST(shape_mesh_appends_road_geometry) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  EXPECT_TRUE(lib.load_p3s(path_join(dir, "GAME1.P3S")));
  const Shape3D* road = lib.find("road");
  EXPECT_TRUE(road != nullptr);
  if (road == nullptr) return;

  MeshBuilder mb;
  const int before = static_cast<int>(mb.vertex_count());
  world::append_shape(mb, *road, world::shape_model(glm::vec3(0.0f), 0.0f),
                      glm::vec4(1.0f));
  EXPECT_TRUE(mb.vertex_count() > before);
  // Vertices and indices must be consistent (multiples of 3 vertices appended).
  EXPECT_TRUE(mb.indices().size() % 3 == 0);
  bool in_range = true;
  for (uint32_t idx : mb.indices()) {
    if (idx >= mb.vertex_count()) in_range = false;
  }
  EXPECT_TRUE(in_range);
}

TEST(shape_mesh_paint_callback_can_skip) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  EXPECT_TRUE(lib.load_p3s(path_join(dir, "GAME1.P3S")));
  const Shape3D* road = lib.find("road");
  if (road == nullptr) return;

  MeshBuilder mb;
  world::append_shape(mb, *road, world::shape_model(glm::vec3(0.0f), 0.0f),
                      [](uint8_t) { return glm::vec4(0.0f); });
  EXPECT_EQ(mb.vertex_count(), 0u);
}
