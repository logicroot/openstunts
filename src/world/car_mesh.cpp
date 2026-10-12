#include "world/car_mesh.h"

#include "core/paths.h"
#include "world/material.h"
#include "world/shape_mesh.h"

namespace s3d::world {

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
                      const PaintColorFn& color_for, int material) {
  const Shape3D* shape = car_lib.find("car1");
  if (shape == nullptr) shape = car_lib.find("car2");
  if (shape == nullptr) return;

  append_shape(mb, *shape, model, color_for, material);
}

}  // namespace s3d::world
