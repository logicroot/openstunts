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
                  const PaintColorFn& color_for, int material, MeshBuilder* line_out) {
  // Pre-transform all vertices once.
  std::vector<glm::vec3> pts(shape.vertices.size());
  for (size_t i = 0; i < shape.vertices.size(); ++i) {
    pts[i] = transform_point(model, shape.vertices[i]);
  }

  for (const auto& prim : shape.primitives) {
    // Select this primitive's paint byte for the requested material slot. The
    // original clamps an out-of-range material to slot 0.
    int slot = material;
    if (slot < 0) slot = 0;
    uint8_t paint = 0;
    if (!prim.paint.empty()) {
      if (slot >= static_cast<int>(prim.paint.size())) slot = 0;
      paint = prim.paint[static_cast<size_t>(slot)];
    }
    const glm::vec4 color = color_for(paint);
    if (color.a <= 0.0f) continue;

    if (prim.kind == PrimKind::Line) {
      if (line_out == nullptr || prim.indices.size() < 2) continue;
      for (size_t i = 0; i + 1 < prim.indices.size(); i += 2) {
        const uint8_t a = prim.indices[i];
        const uint8_t b = prim.indices[i + 1];
        if (a >= pts.size() || b >= pts.size()) continue;
        line_out->add_vertex(pts[a], glm::vec3(0.0f, 1.0f, 0.0f), color);
        line_out->add_vertex(pts[b], glm::vec3(0.0f, 1.0f, 0.0f), color);
      }
      continue;
    }
    if (prim.kind != PrimKind::Polygon) continue;
    if (prim.indices.size() < 3) continue;

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

glm::vec4 track_paint_color(uint8_t p) {
  switch (p) {
    case 19: return {0.30f, 0.30f, 0.33f, 1.0f};  // asphalt
    case 25: return {0.28f, 0.28f, 0.31f, 1.0f};
    case 28: return {0.26f, 0.26f, 0.29f, 1.0f};
    case 22: return {0.31f, 0.31f, 0.34f, 1.0f};
    case 23: return {0.35f, 0.35f, 0.38f, 1.0f};
    case 24: return {0.27f, 0.27f, 0.30f, 1.0f};
    case 21: case 27: case 30: return {0.92f, 0.92f, 0.90f, 1.0f};  // markings
    case 35: return {0.78f, 0.16f, 0.13f, 1.0f};  // red curb
    case 36: return {0.90f, 0.90f, 0.88f, 1.0f};  // white curb
    case 31: case 32: case 33: return {0.62f, 0.63f, 0.66f, 1.0f};  // rails
    case 10: return {0.42f, 0.27f, 0.14f, 1.0f};  // trunk
    case 98: return {0.22f, 0.46f, 0.19f, 1.0f};  // foliage
    case 99: return {0.16f, 0.37f, 0.14f, 1.0f};
    case 100: return {0.15f, 0.38f, 0.68f, 1.0f};  // water
    case 101: case 104: case 105: return {0.34f, 0.56f, 0.26f, 1.0f};  // grass
    case 102: case 103: return {0.29f, 0.50f, 0.22f, 1.0f};  // slope grass
    case 0: return {0.52f, 0.18f, 0.15f, 1.0f};  // barn
    case 4: return {0.80f, 0.78f, 0.72f, 1.0f};
    case 6: return {0.40f, 0.27f, 0.15f, 1.0f};
    case 12: return {0.58f, 0.44f, 0.29f, 1.0f};
    case 15: return {0.85f, 0.85f, 0.83f, 1.0f};
    default: {
      const float v = 0.40f + 0.012f * static_cast<float>(p % 9);
      return {v, v, v * 1.03f, 1.0f};
    }
  }
}

}  // namespace s3d::world
