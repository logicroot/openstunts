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

// A multi-paint shape (the windmill has 4 paint slots) must select the colour
// of the requested material slot. The animation cycles this slot 0..3.
TEST(shape_mesh_material_slot_selects_paint) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  const Shape3D* wind = lib.find("wind");
  EXPECT_TRUE(wind != nullptr);
  if (wind == nullptr) return;
  EXPECT_EQ(wind->num_paints, 4);

  // Encode the resolved paint byte into the vertex colour so slot selection is
  // observable in the output geometry.
  auto paint_color = [](uint8_t paint) -> glm::vec4 {
    return glm::vec4(static_cast<float>(paint) / 255.0f, 0.0f, 0.0f, 1.0f);
  };

  MeshBuilder slot0;
  MeshBuilder slot1;
  world::append_shape(slot0, *wind, world::shape_model(glm::vec3(0.0f), 0.0f), paint_color, 0);
  world::append_shape(slot1, *wind, world::shape_model(glm::vec3(0.0f), 0.0f), paint_color, 1);
  EXPECT_TRUE(slot0.vertex_count() > 0);
  EXPECT_EQ(slot0.vertex_count(), slot1.vertex_count());  // same prims, different colours
  bool differs = false;
  for (size_t i = 0; i < slot0.vertices().size(); ++i) {
    if (slot0.vertices()[i].color.r != slot1.vertices()[i].color.r) differs = true;
  }
  EXPECT_TRUE(differs);
}

// The windmill sail polygons switch between material 113 (the sail) and 118
// (the transparent animation mask); a zero-alpha callback must drop the mask
// triangles, leaving only the lit sail.
TEST(shape_mesh_transparent_paint_is_skipped) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  const Shape3D* wind = lib.find("wind");
  if (wind == nullptr) return;

  MeshBuilder all;
  MeshBuilder lit;
  world::append_shape(all, *wind, world::shape_model(glm::vec3(0.0f), 0.0f),
                      [](uint8_t) { return glm::vec4(1.0f); }, 0);
  world::append_shape(lit, *wind, world::shape_model(glm::vec3(0.0f), 0.0f),
                      [](uint8_t paint) -> glm::vec4 {
                        return paint == 118 ? glm::vec4(0.0f) : glm::vec4(1.0f);
                      },
                      0);
  EXPECT_TRUE(all.vertex_count() > lit.vertex_count());
  EXPECT_TRUE(lit.vertex_count() > 0);
}

// Line primitives (Joe's flashing sign) are skipped by default but collected
// into the line output when one is supplied.
TEST(shape_mesh_collects_line_primitives) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  const Shape3D* rest = lib.find("rest");
  if (rest == nullptr) return;

  MeshBuilder polys;
  MeshBuilder lines;
  world::append_shape(polys, *rest, world::shape_model(glm::vec3(0.0f), 0.0f),
                      [](uint8_t) { return glm::vec4(1.0f); }, 0, &lines);
  EXPECT_TRUE(polys.vertex_count() > 0);   // the diner building
  EXPECT_TRUE(lines.vertex_count() > 0);   // the sign outline
  EXPECT_TRUE(lines.vertex_count() % 2 == 0);  // vertex pairs for GL_LINES

  // Without a line sink the line primitive count must not change the triangles.
  MeshBuilder polys_only;
  world::append_shape(polys_only, *rest, world::shape_model(glm::vec3(0.0f), 0.0f),
                      [](uint8_t) { return glm::vec4(1.0f); }, 0);
  EXPECT_EQ(polys_only.vertex_count(), polys.vertex_count());
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
