#include <cstdint>
#include <string>

#include "assets/shape3d.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/car_mesh.h"
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
                   glm::vec4(0.8f, 0.1f, 0.1f, 1.0f));
  EXPECT_TRUE(mb.vertex_count() > 0);
  EXPECT_TRUE(mb.indices().size() % 3 == 0);
}

// Car paint jobs route to different fills: tyres (38) to the dark tread
// colour, the rear lights (0x2D) to `backlight_color`, everything else to
// `body_color`. The 0x2D sentinel is what shape3d swaps for
// `backlights_paint_override`.
TEST(car_primitive_color_routes_backlights) {
  const glm::vec4 body(0.8f, 0.6f, 0.1f, 1.0f);
  const glm::vec4 light(1.0f, 0.33f, 0.33f, 1.0f);

  EXPECT_TRUE(car_primitive_color(0x2D, body, light) == light);
  EXPECT_TRUE(car_primitive_color(44, body, light) == body);

  // Tyres ignore both caller colours and use the fixed dark tread colour.
  const glm::vec4 tread = car_primitive_color(38, body, light);
  EXPECT_TRUE(tread != body);
  EXPECT_TRUE(tread != light);
  EXPECT_TRUE(tread.r < 0.2f && tread.g < 0.2f && tread.b < 0.2f);
}
