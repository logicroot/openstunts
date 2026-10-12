#include "world/car_mesh.h"

#include <cstdlib>

#include "core/paths.h"
#include "physics/fixed_math.h"
#include "world/material.h"
#include "world/shape_mesh.h"

namespace s3d::world {

void apply_car_wheel_pose(Shape3D* shape, const CarWheelPose& pose) {
  if (shape == nullptr) return;
  std::vector<Vec3i>& v = shape->vertices;
  const int first = kCarWheelFirstVertex;
  const int last = first + kCarWheelVertexCount;
  if (static_cast<int>(v.size()) < last) return;  // no wheel block (car2)

  // Front-wheel pivots (shape3d_load_car_shapes / seg004.asm 3354): the pivot x
  // is the mean of the wheel's outer/inner vertex x (a 16-bit arithmetic shift,
  // i.e. floor), the pivot z is the first vertex's z.
  const int px0 = (v[first + 3].x + v[first + 0].x) >> 1;
  const int pz0 = v[first + 0].z;
  const int px1 = (v[first + 6].x + v[first + 9].x) >> 1;
  const int pz1 = v[first + 6].z;

  // The original halves the steering angle with `sar ax, 1` before taking
  // sin/cos; use the same arithmetic shift so 0x400-unit angles match.
  const int16_t s = fixed::sin_fast(static_cast<unsigned short>(pose.steering_angle >> 1));
  const int16_t c = fixed::cos_fast(static_cast<unsigned short>(pose.steering_angle >> 1));

  // Front wheels (0, 1): the original writes
  //   x' = px + mult(dx, cos) + mult(dz, sin)
  //   z' = pz + mult(dx, sin) + mult(dz, cos)
  // with (dx, dz) = pivot - vertex. `posed` mirrors sub_204AE's angle cache,
  // which is only cleared (and the transform only run) once the steering angle
  // has changed from the cached 0.
  if (pose.posed) {
    for (int wheel = 0; wheel < 2; ++wheel) {
      const int base = first + wheel * 6;
      const int px = (wheel == 0) ? px0 : px1;
      const int pz = (wheel == 0) ? pz0 : pz1;
      for (int i = 0; i < 6; ++i) {
        const int dx = px - v[base + i].x;
        const int dz = pz - v[base + i].z;
        const int nx = px + fixed::multiply_and_scale(static_cast<int16_t>(dx), c) +
                       fixed::multiply_and_scale(static_cast<int16_t>(dz), s);
        const int nz = pz + fixed::multiply_and_scale(static_cast<int16_t>(dx), s) +
                       fixed::multiply_and_scale(static_cast<int16_t>(dz), c);
        v[base + i].x = static_cast<int16_t>(nx);
        v[base + i].z = static_cast<int16_t>(nz);
      }
    }
  }

  // All four wheels: y = rest_y - (|car_rc2| >> 6). The original computes
  // `labs(labs(car_rc2[j]) >> 6)` and offsets each wheel's six vertices.
  for (int wheel = 0; wheel < 4; ++wheel) {
    const int offset = std::abs(static_cast<int>(pose.suspension[wheel])) >> 6;
    const int base = first + wheel * 6;
    for (int i = 0; i < 6; ++i) {
      v[base + i].y = static_cast<int16_t>(v[base + i].y - offset);
    }
  }
}

bool load_car_archive(const std::string& asset_dir, const std::string& car_id,
                      ShapeLibrary* out) {
  if (asset_dir.empty() || car_id.empty()) return false;
  return out->load_p3s(path_join(asset_dir, "ST" + car_id + ".P3S"));
}

glm::vec4 car_primitive_color(uint8_t paint, const Palette& palette,
                              const glm::vec4& backlight_color) {
  if (paint == kCarBacklightPaint) return backlight_color;
  return material_color(paint, palette);
}

void append_car_model(MeshBuilder& mb, const ShapeLibrary& car_lib, const glm::mat4& model,
                      const PaintColorFn& color_for, int material,
                      const CarWheelPose* wheel_pose) {
  const Shape3D* shape = car_lib.find("car1");
  if (shape == nullptr) shape = car_lib.find("car2");
  if (shape == nullptr) return;

  if (wheel_pose != nullptr &&
      static_cast<int>(shape->vertices.size()) >= kCarWheelFirstVertex + kCarWheelVertexCount) {
    // Animate the wheels on a copy so the loaded shape stays at its rest pose.
    Shape3D posed = *shape;
    apply_car_wheel_pose(&posed, *wheel_pose);
    append_shape(mb, posed, model, color_for, material);
    return;
  }
  append_shape(mb, *shape, model, color_for, material);
}

}  // namespace s3d::world
