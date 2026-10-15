#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "assets/shape3d.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"

using namespace s3d;

// Primitive type 12 is the original's dedicated wheel (`_primtype_wheel`,
// primtypetab[12] = 3), authored as a six-vertex wedge that only covers the
// upper part of the wheel. The port rebuilds it as a round low-poly wheel
// about the axle (kCarWheelSides facets plus hub discs) so the wheels read as
// round and stay opaque, instead of the flat triangle the raw data describes.
// The authored dark-grey material paints the tyre band and tread, and the hub
// inside it is a lightened grey.
TEST(shape_mesh_wheel_primitive_emits_round_tyre) {
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
                      [](uint8_t) -> glm::vec4 { return glm::vec4(0.2f, 0.2f, 0.2f, 1.0f); });

  // Two hub fans of (kCarWheelSides - 2) triangles, two tyre sidewall bands of
  // kCarWheelSides quads (two triangles each) and a kCarWheelSides-quad tread.
  const size_t sides = static_cast<size_t>(world::kCarWheelSides);
  const size_t tri_count =
      (sides - 2) * 2 + sides * 2 * 2 + sides * 2;
  EXPECT_EQ(mb.vertex_count(), tri_count * 3);
  EXPECT_EQ(mb.indices().size(), tri_count * 3);

  // The axle is the X axis: inner face at x=0, outer at x=1. The smallest
  // circle enclosing the authored rim points (0,0), (1,0), (0,1) is centred at
  // (0.5, 0.5) with radius sqrt(0.5), so every emitted vertex sits on either
  // the tyre cylinder or the smaller hub cylinder inside it.
  const float radius = std::sqrt(0.5f);
  const float rim_radius = radius * world::kCarWheelRimRatio;
  bool saw_tyre = false;
  bool saw_rim = false;
  bool off_cylinder = false;
  const std::vector<Vertex>& verts = mb.vertices();
  for (const Vertex& v : verts) {
    EXPECT_TRUE(std::abs(v.position.x - 0.0f) < 1e-3f ||
                std::abs(v.position.x - 1.0f) < 1e-3f);
    const float dy = v.position.y - 0.5f;
    const float dz = v.position.z - 0.5f;
    const float r = std::sqrt(dy * dy + dz * dz);
    if (std::abs(r - radius) < 1e-3f) {
      saw_tyre = true;
    } else if (std::abs(r - rim_radius) < 1e-3f) {
      saw_rim = true;
    } else {
      off_cylinder = true;
    }
  }
  EXPECT_TRUE(saw_tyre);
  EXPECT_TRUE(saw_rim);
  EXPECT_TRUE(!off_cylinder);

  // The tyre (authored dark grey) and the rim (a lightened grey) must be
  // distinct, so the wheel reads as a grey hub inside a dark tyre.
  bool saw_dark = false;
  bool saw_light = false;
  for (const Vertex& v : verts) {
    if (v.color.r < 0.3f) saw_dark = true;
    if (v.color.r > 0.4f) saw_light = true;
  }
  EXPECT_TRUE(saw_dark);
  EXPECT_TRUE(saw_light);

  // The tyre is closed: every edge is shared by exactly two triangles (the
  // vertices are unindexed, so merge coincident ones first).
  std::map<std::array<int, 3>, int> ids;
  const auto vertex_id = [&](const glm::vec3& p) {
    const std::array<int, 3> key = {static_cast<int>(std::lround(p.x * 1024.0f)),
                                    static_cast<int>(std::lround(p.y * 1024.0f)),
                                    static_cast<int>(std::lround(p.z * 1024.0f))};
    const auto it = ids.find(key);
    if (it != ids.end()) return it->second;
    const int id = static_cast<int>(ids.size());
    ids.emplace(key, id);
    return id;
  };
  std::map<std::pair<int, int>, int> edges;
  for (size_t t = 0; t + 2 < verts.size(); t += 3) {
    const int a = vertex_id(verts[t].position);
    const int b = vertex_id(verts[t + 1].position);
    const int c = vertex_id(verts[t + 2].position);
    for (const auto& e : {std::pair<int, int>{a, b}, {b, c}, {c, a}}) {
      edges[{std::min(e.first, e.second), std::max(e.first, e.second)}]++;
    }
  }
  for (const auto& kv : edges) {
    EXPECT_EQ(kv.second, 2);
  }
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

// An element's own markings (the yellow line painted down a road) are authored
// coincident with the road surface, so a z-buffered renderer z-fights them and
// the line flickers as the camera moves. append_shape recognises a marking as a
// small polygon lying on a much larger one of the same shape and routes it to
// `decal_out`, so only the surface is left in `mb`.
TEST(shape_mesh_routes_road_markings_to_decal_buffer) {
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

  // Encode the resolved paint byte into the colour so the two groups are
  // observable: the road surface is paint 19, its dashes are paint 21.
  auto paint_color = [](uint8_t paint) -> glm::vec4 {
    return glm::vec4(static_cast<float>(paint), 0.0f, 0.0f, 1.0f);
  };
  const glm::mat4 model = world::shape_model(glm::vec3(0.0f), 0.0f);

  MeshBuilder surface;
  MeshBuilder decal;
  world::append_shape(surface, *road, model, paint_color, 0, nullptr, &decal);

  EXPECT_TRUE(surface.vertex_count() > 0);
  EXPECT_TRUE(decal.vertex_count() > 0);
  bool surface_is_dashes = false;
  for (const Vertex& v : surface.vertices()) {
    if (v.color.r > 20.0f) surface_is_dashes = true;
  }
  EXPECT_TRUE(!surface_is_dashes);  // no paint-21 dash left in the surface
  bool decal_is_surface = false;
  for (const Vertex& v : decal.vertices()) {
    if (v.color.r < 20.0f) decal_is_surface = true;
  }
  EXPECT_TRUE(!decal_is_surface);  // no paint-19 surface in the decal buffer

  // Splitting the shape must not drop or duplicate any geometry.
  MeshBuilder whole;
  world::append_shape(whole, *road, model, paint_color, 0);
  EXPECT_EQ(surface.vertex_count() + decal.vertex_count(), whole.vertex_count());
}

// The authored markings are not all wound the same way round -- shape 30's
// fourth dash runs backwards -- so a marking must take the normal of the surface
// it lies on. Otherwise the port's lighting makes that one dash darker than its
// neighbours.
TEST(shape_mesh_lights_road_markings_like_their_surface) {
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

  auto paint_color = [](uint8_t paint) -> glm::vec4 {
    return glm::vec4(static_cast<float>(paint), 0.0f, 0.0f, 1.0f);
  };
  const glm::mat4 model = world::shape_model(glm::vec3(0.0f), 0.0f);
  MeshBuilder surface;
  MeshBuilder decal;
  world::append_shape(surface, *road, model, paint_color, 0, nullptr, &decal);

  // The road is flat, so its surface and the dashes painted on it all face up,
  // whichever way round the particular dash happened to be wound.
  EXPECT_TRUE(decal.vertex_count() > 0);
  for (const Vertex& v : decal.vertices()) {
    EXPECT_TRUE(v.normal.y > 0.9f);
  }
  for (const Vertex& v : surface.vertices()) {
    EXPECT_TRUE(v.normal.y > 0.9f);
  }
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

// A patterned material is carried per vertex as a screen-space dither mask so
// the fragment shader can stipple the polygon (the original's
// draw_patterned_lines). The mask is looked up from the paint index; without a
// lookup function every vertex stays flat (0).
TEST(shape_mesh_stores_pattern_per_vertex) {
  Shape3D shape;
  shape.vertices = {Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{1, 0, 1}, Vec3i{0, 0, 1}};
  ShapePrimitive prim;
  prim.kind = PrimKind::Polygon;
  prim.paint = {22};
  prim.indices = {0, 1, 2, 3};
  shape.primitives.push_back(prim);

  const auto paint_color = [](uint8_t) -> glm::vec4 { return glm::vec4(1.0f); };

  MeshBuilder flat;
  world::append_shape(flat, shape, glm::mat4(1.0f), paint_color);
  EXPECT_TRUE(flat.vertex_count() > 0);
  for (const Vertex& v : flat.vertices()) EXPECT_TRUE(v.pattern == 0.0f);

  MeshBuilder patterned;
  world::append_shape(patterned, shape, glm::mat4(1.0f), paint_color, 0, nullptr, nullptr,
                      [](uint8_t paint) -> uint16_t {
                        return paint == 22 ? 0x77DDu : 0u;
                      });
  EXPECT_EQ(patterned.vertex_count(), flat.vertex_count());
  for (const Vertex& v : patterned.vertices()) EXPECT_TRUE(v.pattern == 30685.0f);
}
