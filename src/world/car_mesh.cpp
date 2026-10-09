#include "world/car_mesh.h"

#include "core/paths.h"
#include "world/shape_mesh.h"

namespace s3d::world {

namespace {

// Wheel/chassis primitives in the car archives use paint index 38; the body
// picks one of the livery paints (44..79) per material. Until the material
// table is loaded we draw the tyres dark and everything else in `body_color`.
constexpr uint8_t kWheelPaint = 38;

}  // namespace

bool load_car_archive(const std::string& asset_dir, const std::string& car_id,
                      ShapeLibrary* out) {
  if (asset_dir.empty() || car_id.empty()) return false;
  return out->load_p3s(path_join(asset_dir, "ST" + car_id + ".P3S"));
}

void append_car_model(MeshBuilder& mb, const ShapeLibrary& car_lib, const glm::mat4& model,
                      const glm::vec4& body_color) {
  const Shape3D* shape = car_lib.find("car1");
  if (shape == nullptr) shape = car_lib.find("car2");
  if (shape == nullptr) return;

  const glm::vec4 tread(0.10f, 0.10f, 0.11f, 1.0f);
  append_shape(mb, *shape, model, [&](uint8_t paint) -> glm::vec4 {
    return paint == kWheelPaint ? tread : body_color;
  });
}

}  // namespace s3d::world
