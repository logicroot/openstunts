#pragma once
// Helper for assembling vertex/index data for world geometry.

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

#include "render/mesh.h"

namespace s3d {

class MeshBuilder {
 public:
  void clear() {
    vertices_.clear();
    indices_.clear();
  }

  size_t vertex_count() const { return vertices_.size(); }

  uint32_t add_vertex(const glm::vec3& p, const glm::vec3& n, const glm::vec4& c,
                      const glm::vec2& uv = glm::vec2(0.0f), float pattern = 0.0f) {
    vertices_.push_back(Vertex{p, n, c, uv, pattern});
    return static_cast<uint32_t>(vertices_.size() - 1);
  }

  void add_triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                    const glm::vec4& color, const glm::vec3& normal = glm::vec3(0.0f),
                    float pattern = 0.0f) {
    const glm::vec3 n = (normal == glm::vec3(0.0f))
                            ? glm::normalize(glm::cross(b - a, c - a))
                            : normal;
    const uint32_t base = static_cast<uint32_t>(vertices_.size());
    add_vertex(a, n, color, glm::vec2(0.0f), pattern);
    add_vertex(b, n, color, glm::vec2(0.0f), pattern);
    add_vertex(c, n, color, glm::vec2(0.0f), pattern);
    indices_.insert(indices_.end(), {base, base + 1, base + 2});
  }

  // Quad given in counter-clockwise order (a-b-c-d). Split into two triangles.
  void add_quad(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                const glm::vec3& d, const glm::vec4& color,
                const glm::vec3& normal = glm::vec3(0.0f)) {
    const glm::vec3 n = (normal == glm::vec3(0.0f))
                            ? glm::normalize(glm::cross(b - a, d - a))
                            : normal;
    const uint32_t base = static_cast<uint32_t>(vertices_.size());
    add_vertex(a, n, color);
    add_vertex(b, n, color);
    add_vertex(c, n, color);
    add_vertex(d, n, color);
    indices_.insert(indices_.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }

  // Axis-aligned box from min to max.
  void add_box(const glm::vec3& mn, const glm::vec3& mx, const glm::vec4& color) {
    const glm::vec3 p000(mn.x, mn.y, mn.z), p100(mx.x, mn.y, mn.z);
    const glm::vec3 p110(mx.x, mx.y, mn.z), p010(mn.x, mx.y, mn.z);
    const glm::vec3 p001(mn.x, mn.y, mx.z), p101(mx.x, mn.y, mx.z);
    const glm::vec3 p111(mx.x, mx.y, mx.z), p011(mn.x, mx.y, mx.z);
    add_quad(p010, p011, p111, p110, color, {0, 1, 0});   // top
    add_quad(p000, p100, p101, p001, color, {0, -1, 0});  // bottom
    add_quad(p001, p101, p111, p011, color, {0, 0, 1});   // front (+z)
    add_quad(p100, p000, p010, p110, color, {0, 0, -1});  // back (-z)
    add_quad(p000, p001, p011, p010, color, {-1, 0, 0});  // left
    add_quad(p101, p100, p110, p111, color, {1, 0, 0});   // right
  }

  Mesh to_mesh() const {
    Mesh m;
    m.upload(vertices_, indices_);
    return m;
  }

  const std::vector<Vertex>& vertices() const { return vertices_; }
  const std::vector<uint32_t>& indices() const { return indices_; }

 private:
  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;
};

}  // namespace s3d
