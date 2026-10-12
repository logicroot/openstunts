#include <cstdint>
#include <string>

#include "assets/palette.h"
#include "assets/shape3d.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/car_mesh.h"
#include "world/material.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"

using namespace s3d;
using namespace s3d::world;

namespace {

struct Bounds {
  int min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
};

Bounds vertex_bounds(const Shape3D& s) {
  Bounds b;
  b.min_x = b.min_y = b.min_z = 32767;
  b.max_x = b.max_y = b.max_z = -32768;
  for (const Vec3i& v : s.vertices) {
    if (v.x < b.min_x) b.min_x = v.x;
    if (v.x > b.max_x) b.max_x = v.x;
    if (v.y < b.min_y) b.min_y = v.y;
    if (v.y > b.max_y) b.max_y = v.y;
    if (v.z < b.min_z) b.min_z = v.z;
    if (v.z > b.max_z) b.max_z = v.z;
  }
  return b;
}

}  // namespace

// The car archive ships three shapes: `car0` is the 20x showroom body, while
// `car1`/`car2` are the racing-scale bodies used on track.
TEST(car_archive_exposes_racing_shapes) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) {
    std::printf("  (skipped: STCOUN.P3S not found)\n");
    return;
  }
  const Shape3D* car0 = lib.find("car0");
  const Shape3D* car1 = lib.find("car1");
  const Shape3D* car2 = lib.find("car2");
  EXPECT_TRUE(car0 != nullptr);
  EXPECT_TRUE(car1 != nullptr);
  EXPECT_TRUE(car2 != nullptr);
  if (car0 == nullptr || car1 == nullptr || car2 == nullptr) return;

  // car1 is the full-detail racing body including the four wheels.
  const Bounds b1 = vertex_bounds(*car1);
  EXPECT_TRUE(b1.max_x - b1.min_x < 100);   // ~40 units wide
  EXPECT_TRUE(b1.max_z - b1.min_z < 200);   // ~100 units long
  EXPECT_TRUE(!car1->primitives.empty());
  EXPECT_TRUE(!car1->vertices.empty());

  // The showroom model is authored at ~20x the racing scale.
  const Bounds b0 = vertex_bounds(*car0);
  EXPECT_TRUE(b0.max_x - b0.min_x > 10 * (b1.max_x - b1.min_x));
}

TEST(append_car_model_emits_triangles) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) return;
  if (lib.find("car1") == nullptr) return;

  MeshBuilder mb;
  append_car_model(mb, lib, shape_model({0.0f, 0.0f, 0.0f}, 0.0f),
                   [](uint8_t) { return glm::vec4(0.8f, 0.1f, 0.1f, 1.0f); });
  EXPECT_TRUE(mb.vertex_count() > 0);
  EXPECT_TRUE(mb.indices().size() % 3 == 0);
}

// Car paint jobs are materials, resolved through the original material table
// (material_color_list) exactly like the track. The 0x2D sentinel is not a
// material: shape3d swaps it for `backlights_paint_override`.
TEST(car_primitive_color_samples_material_table) {
  Palette pal;
  pal.rgb8[150][0] = 0;
  pal.rgb8[150][1] = 4;
  pal.rgb8[150][2] = 255;
  const glm::vec4 light(1.0f, 0.33f, 0.33f, 1.0f);

  // Material 51 is one of the livery body paints of the retail cars; the table
  // maps it to palette index 150.
  EXPECT_EQ(material_color_index(51), 150);
  const glm::vec4 body = car_primitive_color(51, pal, light);
  EXPECT_TRUE(body == glm::vec4(0.0f, 4.0f / 255.0f, 255.0f / 255.0f, 1.0f));

  // The rear-light sentinel never samples the table.
  EXPECT_TRUE(car_primitive_color(kCarBacklightPaint, pal, light) == light);
}

// `append_car_model`'s material argument picks the livery slot
// (gameconfig.game_playermaterial), i.e. one of the shape's paint variants:
// the retail bodies carry 7, so slot 1 paints a different bodywork colour than
// slot 0.
TEST(car_append_selects_livery_slot) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) return;
  if (lib.find("car1") == nullptr) return;
  Palette pal;
  if (!load_vga_palette(path_join(dir, "SDMAIN.PVS"), &pal)) {
    std::printf("  (skipped: no palette)\n");
    return;
  }

  const glm::vec4 light(0.67f, 0.0f, 0.0f, 1.0f);
  const auto color = [&pal, light](uint8_t paint) {
    return car_primitive_color(paint, pal, light);
  };
  MeshBuilder m0;
  append_car_model(m0, lib, glm::mat4(1.0f), color, 0);
  MeshBuilder m1;
  append_car_model(m1, lib, glm::mat4(1.0f), color, 1);

  const std::vector<Vertex>& v0 = m0.vertices();
  const std::vector<Vertex>& v1 = m1.vertices();
  EXPECT_EQ(v0.size(), v1.size());
  bool differs = false;
  if (v0.size() == v1.size()) {
    for (size_t i = 0; i < v0.size(); ++i) {
      if (v0[i].color != v1[i].color) {
        differs = true;
        break;
      }
    }
  }
  EXPECT_TRUE(differs);
}
