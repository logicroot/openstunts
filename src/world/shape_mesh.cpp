#include "world/shape_mesh.h"

#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace s3d::world {

namespace {

glm::vec3 transform_point(const glm::mat4& m, const Vec3i& v) {
  return glm::vec3(m * glm::vec4(static_cast<float>(v.x), static_cast<float>(v.y),
                                 static_cast<float>(v.z), 1.0f));
}

glm::vec3 face_normal(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
  const glm::vec3 n = glm::cross(b - a, c - a);
  const float len = glm::length(n);
  return (len > 1e-6f) ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
}

void append_polygon(MeshBuilder& mb, const std::vector<glm::vec3>& pts,
                    const glm::vec4& color) {
  if (pts.size() < 3) return;
  const glm::vec3 n = face_normal(pts[0], pts[1], pts[2]);
  // Triangle fan.
  for (size_t i = 1; i + 1 < pts.size(); ++i) {
    mb.add_triangle(pts[0], pts[i], pts[i + 1], color, n);
  }
}

}  // namespace

void append_shape(MeshBuilder& mb, const Shape3D& shape, const glm::mat4& model,
                  const glm::vec4& color) {
  append_shape(mb, shape, model, [&color](uint8_t) { return color; });
}

void append_shape(MeshBuilder& mb, const Shape3D& shape, const glm::mat4& model,
                  const PaintColorFn& color_for) {
  // Pre-transform all vertices once.
  std::vector<glm::vec3> pts(shape.vertices.size());
  for (size_t i = 0; i < shape.vertices.size(); ++i) {
    pts[i] = transform_point(model, shape.vertices[i]);
  }

  for (const auto& prim : shape.primitives) {
    if (prim.kind != PrimKind::Polygon) continue;
    if (prim.indices.size() < 3) continue;
    const uint8_t paint = prim.paint.empty() ? 0 : prim.paint[0];
    const glm::vec4 color = color_for(paint);
    if (color.a <= 0.0f) continue;

    std::vector<glm::vec3> poly;
    poly.reserve(prim.indices.size());
    for (uint8_t idx : prim.indices) {
      if (idx < pts.size()) poly.push_back(pts[idx]);
    }
    append_polygon(mb, poly, color);
  }
}

glm::mat4 shape_model(const glm::vec3& translation, float yaw_deg) {
  glm::mat4 m = glm::translate(glm::mat4(1.0f), translation);
  return glm::rotate(m, glm::radians(yaw_deg), glm::vec3(0.0f, 1.0f, 0.0f));
}

}  // namespace s3d::world
