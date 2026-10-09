#pragma once
// Simple interleaved vertex mesh (position, normal, color, uv).

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace s3d {

struct Vertex {
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
  glm::vec4 color{1.0f};
  glm::vec2 uv{0.0f};
};

class Mesh {
 public:
  Mesh() = default;
  ~Mesh();

  Mesh(const Mesh&) = delete;
  Mesh& operator=(const Mesh&) = delete;
  Mesh(Mesh&& other) noexcept;
  Mesh& operator=(Mesh&& other) noexcept;

  // Uploads vertex (and optional index) data. Safe to call repeatedly.
  void upload(const std::vector<Vertex>& vertices);
  void upload(const std::vector<Vertex>& vertices,
              const std::vector<uint32_t>& indices);
  void destroy();

  void draw() const;
  void draw_lines() const;

  bool empty() const { return vertex_count_ == 0; }
  size_t vertex_count() const { return vertex_count_; }
  size_t index_count() const { return index_count_; }

 private:
  unsigned vao_ = 0;
  unsigned vbo_ = 0;
  unsigned ibo_ = 0;
  size_t vertex_count_ = 0;
  size_t index_count_ = 0;
};

}  // namespace s3d
