#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <set>
#include <string>

#include "assets/shape3d.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"

using namespace s3d;

// Primitive type 12 is the original's dedicated wheel (`_primtype_wheel`,
// primtypetab[12] = 3): the six referenced vertices form two quads that share
// the edge (v0,v3), and the software renderer fills whichever quad faces the
// camera. A z-buffered renderer emits both quads instead of a single
// six-vertex fan, which would be self-intersecting/degenerate.
TEST(shape_mesh_wheel_primitive_emits_two_quads) {
  Shape3D shape;
  shape.vertices = {Vec3i{0, 0, 0}, Vec3i{0, 1, 0}, Vec3i{0, 0, 1},
                    Vec3i{1, 0, 0}, Vec3i{1, 1, 0}, Vec3i{1, 0, 1}};
  ShapePrimitive prim;
  prim.type = 12;
  prim.kind = PrimKind::Polygon;
  prim.paint = {1};
  prim.indices = {0, 1, 2, 3, 4, 5};
  shape.primitives.push_back(prim);

  MeshBuilder mb;
  world::append_shape(mb, shape, glm::mat4(1.0f),
                      [](uint8_t) -> glm::vec4 { return glm::vec4(1.0f); });

  // Two quads -> two triangles each -> 12 vertices / 12 indices.
  EXPECT_EQ(mb.vertex_count(), static_cast<size_t>(12));
  EXPECT_EQ(mb.indices().size(), static_cast<size_t>(12));

  const std::vector<Vertex>& verts = mb.vertices();
  const auto src_index = [&](const glm::vec3& p) -> int {
    for (size_t i = 0; i < shape.vertices.size(); ++i) {
      const Vec3i& s = shape.vertices[i];
      if (p == glm::vec3(s.x, s.y, s.z)) return static_cast<int>(i);
    }
    return -1;
  };
  std::set<std::array<int, 3>> tris;
  for (size_t t = 0; t + 2 < verts.size(); t += 3) {
    std::array<int, 3> tri = {src_index(verts[t].position),
                              src_index(verts[t + 1].position),
                              src_index(verts[t + 2].position)};
    std::sort(tri.begin(), tri.end());
    tris.insert(tri);
  }
  // Quad [0,1,2,3] then quad [3,4,5,0].
  EXPECT_EQ(tris.count(std::array<int, 3>{0, 1, 2}), static_cast<size_t>(1));
  EXPECT_EQ(tris.count(std::array<int, 3>{0, 2, 3}), static_cast<size_t>(1));
  EXPECT_EQ(tris.count(std::array<int, 3>{3, 4, 5}), static_cast<size_t>(1));
  EXPECT_EQ(tris.count(std::array<int, 3>{0, 3, 5}), static_cast<size_t>(1));
  // The old six-vertex fan would have produced these instead.
  EXPECT_EQ(tris.count(std::array<int, 3>{0, 3, 4}), static_cast<size_t>(0));
  EXPECT_EQ(tris.count(std::array<int, 3>{0, 4, 5}), static_cast<size_t>(0));
}

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
