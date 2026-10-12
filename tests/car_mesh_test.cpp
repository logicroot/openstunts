#include <array>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

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

namespace {

// The six vertices of wheel `w` of a car1 shape (0..3: front-left, front-right,
// rear-right, rear-left).
std::array<Vec3i, 6> wheel_verts(const Shape3D& s, int w) {
  std::array<Vec3i, 6> out{};
  for (int i = 0; i < 6; ++i) {
    out[static_cast<size_t>(i)] =
        s.vertices[static_cast<size_t>(kCarWheelFirstVertex + w * 6 + i)];
  }
  return out;
}

}  // namespace

// sub_204AE is a no-op at rest: steering 0 and no suspension travel leave the
// authored mesh untouched (the original skips the block while the cached angle
// is unchanged).
TEST(car_wheel_pose_rest_is_identity) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) return;
  const Shape3D* car1 = lib.find("car1");
  if (car1 == nullptr) return;
  if (car1->vertices.size() <
      static_cast<size_t>(kCarWheelFirstVertex + kCarWheelVertexCount)) {
    return;
  }

  Shape3D posed = *car1;
  apply_car_wheel_pose(&posed, CarWheelPose{});
  for (size_t i = 0; i < car1->vertices.size(); ++i) {
    EXPECT_TRUE(posed.vertices[i].x == car1->vertices[i].x);
    EXPECT_TRUE(posed.vertices[i].y == car1->vertices[i].y);
    EXPECT_TRUE(posed.vertices[i].z == car1->vertices[i].z);
  }
}

// Every wheel drops by |car_rc2| >> 6 world units; a car2 (no wheel block) is
// left unchanged.
TEST(car_wheel_pose_suspension_drops_wheels) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) return;
  const Shape3D* car1 = lib.find("car1");
  if (car1 == nullptr) return;
  if (car1->vertices.size() <
      static_cast<size_t>(kCarWheelFirstVertex + kCarWheelVertexCount)) {
    return;
  }

  CarWheelPose pose;
  const int16_t travel[4] = {64, 128, 192, 256};
  for (int i = 0; i < 4; ++i) pose.suspension[i] = travel[i];
  Shape3D posed = *car1;
  apply_car_wheel_pose(&posed, pose);

  for (int w = 0; w < 4; ++w) {
    const int drop = travel[w] >> 6;
    for (int i = 0; i < 6; ++i) {
      const size_t idx = static_cast<size_t>(kCarWheelFirstVertex + w * 6 + i);
      EXPECT_TRUE(posed.vertices[idx].y == car1->vertices[idx].y - drop);
      // x/z are only touched by the steering block, which is skipped here.
      EXPECT_TRUE(posed.vertices[idx].x == car1->vertices[idx].x);
      EXPECT_TRUE(posed.vertices[idx].z == car1->vertices[idx].z);
    }
  }
}

// A full steering turn (0x400 = 360 degrees, i.e. a half-angle of 180 degrees)
// makes sub_204AE the identity: sin = 0 and cos = -1, so
// x' = px - (px - x) = x and z' = pz - (pz - z) = z, leaving the whole car
// unchanged.
TEST(car_wheel_pose_full_turn_is_identity) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) return;
  const Shape3D* car1 = lib.find("car1");
  if (car1 == nullptr) return;
  if (car1->vertices.size() <
      static_cast<size_t>(kCarWheelFirstVertex + kCarWheelVertexCount)) {
    return;
  }

  CarWheelPose pose;
  pose.posed = true;
  pose.steering_angle = 0x400;
  Shape3D posed = *car1;
  apply_car_wheel_pose(&posed, pose);
  for (size_t i = 0; i < car1->vertices.size(); ++i) {
    EXPECT_TRUE(posed.vertices[i].x == car1->vertices[i].x);
    EXPECT_TRUE(posed.vertices[i].y == car1->vertices[i].y);
    EXPECT_TRUE(posed.vertices[i].z == car1->vertices[i].z);
  }
}

// Steering rotates only the two front wheels (0, 1): their six vertices move in
// x/z while the rear wheels (2, 3) keep their x/z untouched and every wheel's y
// is left to the suspension step.
TEST(car_wheel_pose_steers_front_wheels) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) return;
  const Shape3D* car1 = lib.find("car1");
  if (car1 == nullptr) return;
  if (car1->vertices.size() <
      static_cast<size_t>(kCarWheelFirstVertex + kCarWheelVertexCount)) {
    return;
  }

  CarWheelPose pose;
  pose.posed = true;
  pose.steering_angle = 0x100;  // quarter turn of the steering wheel (90 degrees)
  Shape3D posed = *car1;
  apply_car_wheel_pose(&posed, pose);

  for (int w = 0; w < 4; ++w) {
    const auto before = wheel_verts(*car1, w);
    const auto after = wheel_verts(posed, w);
    if (w < 2) {
      bool moved = false;
      for (int i = 0; i < 6; ++i) {
        if (after[static_cast<size_t>(i)].x != before[static_cast<size_t>(i)].x ||
            after[static_cast<size_t>(i)].z != before[static_cast<size_t>(i)].z) {
          moved = true;
        }
        EXPECT_TRUE(after[static_cast<size_t>(i)].y == before[static_cast<size_t>(i)].y);
      }
      EXPECT_TRUE(moved);
    } else {
      // Rear wheels: no steering transform.
      for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(after[static_cast<size_t>(i)].x == before[static_cast<size_t>(i)].x);
        EXPECT_TRUE(after[static_cast<size_t>(i)].z == before[static_cast<size_t>(i)].z);
      }
    }
  }
}

// A steering angle whose half rounds to zero (|a| == 1) makes sub_204AE a point
// reflection about each front-wheel pivot: the sine is 0 and the cosine is 1, so
//   x' = px + (px - x) = 2*px - x,  z' = pz + (pz - z) = 2*pz - z.
// The pivots are derived exactly like shape3d_load_car_shapes.
TEST(car_wheel_pose_tiny_angle_reflects_about_pivot) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) return;
  const Shape3D* car1 = lib.find("car1");
  if (car1 == nullptr) return;
  if (car1->vertices.size() <
      static_cast<size_t>(kCarWheelFirstVertex + kCarWheelVertexCount)) {
    return;
  }

  const std::vector<Vec3i>& rest = car1->vertices;
  const int base = kCarWheelFirstVertex;
  const int px[2] = {(rest[base + 3].x + rest[base + 0].x) >> 1,
                     (rest[base + 9].x + rest[base + 6].x) >> 1};
  const int pz[2] = {rest[base + 0].z, rest[base + 6].z};

  CarWheelPose pose;
  pose.posed = true;
  pose.steering_angle = 1;
  Shape3D posed = *car1;
  apply_car_wheel_pose(&posed, pose);

  for (int w = 0; w < 2; ++w) {
    for (int i = 0; i < 6; ++i) {
      const size_t idx = static_cast<size_t>(base + w * 6 + i);
      EXPECT_TRUE(posed.vertices[idx].x == 2 * px[w] - rest[idx].x);
      EXPECT_TRUE(posed.vertices[idx].z == 2 * pz[w] - rest[idx].z);
      EXPECT_TRUE(posed.vertices[idx].y == rest[idx].y);
    }
  }
  // Rear wheels are untouched by the steering transform.
  for (size_t idx = static_cast<size_t>(base + 12); idx < static_cast<size_t>(base + 24); ++idx) {
    EXPECT_TRUE(posed.vertices[idx].x == rest[idx].x);
    EXPECT_TRUE(posed.vertices[idx].z == rest[idx].z);
  }
}

// append_car_model threads the pose through to the emitted geometry: the same
// vertex count, but the wheel triangles sit at the steered positions.
TEST(car_append_applies_wheel_pose) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  if (!load_car_archive(dir, "COUN", &lib)) return;
  if (lib.find("car1") == nullptr) return;

  const auto color = [](uint8_t) { return glm::vec4(0.8f, 0.1f, 0.1f, 1.0f); };
  MeshBuilder rest;
  append_car_model(rest, lib, glm::mat4(1.0f), color, 0, nullptr);
  CarWheelPose pose;
  pose.posed = true;
  pose.steering_angle = 0x180;
  MeshBuilder steered;
  append_car_model(steered, lib, glm::mat4(1.0f), color, 0, &pose);

  EXPECT_EQ(rest.vertex_count(), steered.vertex_count());
  bool differs = false;
  const std::vector<Vertex>& a = rest.vertices();
  const std::vector<Vertex>& b = steered.vertices();
  if (a.size() == b.size()) {
    for (size_t i = 0; i < a.size(); ++i) {
      if (a[i].position != b[i].position) {
        differs = true;
        break;
      }
    }
  }
  EXPECT_TRUE(differs);
}
