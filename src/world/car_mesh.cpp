#include "world/car_mesh.h"

#include "core/paths.h"
#include "world/shape_mesh.h"

namespace s3d::world {

namespace {

// Wheel/chassis primitives in the car archives use paint index 38; the body
// picks one of the livery paints (44..79). Player livery selection is not
// modelled yet, so the tyres are drawn dark and everything else in the
// caller-supplied `body_color`.
constexpr uint8_t kWheelPaint = 38;

// Paint job 0x2D marks the rear light polygons. The original never paints that
// livery: shape3d replaces it with `backlights_paint_override` (see
// seg006.asm loc_25D3C / c/shape3d.c loc_25D3C).
constexpr uint8_t kBacklightPaint = 0x2D;

}  // namespace

bool load_car_archive(const std::string& asset_dir, const std::string& car_id,
                      ShapeLibrary* out) {
  if (asset_dir.empty() || car_id.empty()) return false;
  return out->load_p3s(path_join(asset_dir, "ST" + car_id + ".P3S"));
}

glm::vec4 car_primitive_color(uint8_t paint, const glm::vec4& body_color,
                              const glm::vec4& backlight_color) {
  if (paint == kWheelPaint) return glm::vec4(0.10f, 0.10f, 0.11f, 1.0f);
  if (paint == kBacklightPaint) return backlight_color;
  return body_color;
}

void append_car_model(MeshBuilder& mb, const ShapeLibrary& car_lib, const glm::mat4& model,
                      const glm::vec4& body_color, const glm::vec4& backlight_color) {
  const Shape3D* shape = car_lib.find("car1");
  if (shape == nullptr) shape = car_lib.find("car2");
  if (shape == nullptr) return;

  append_shape(mb, *shape, model, [&](uint8_t paint) -> glm::vec4 {
    return car_primitive_color(paint, body_color, backlight_color);
  });
}

}  // namespace s3d::world
