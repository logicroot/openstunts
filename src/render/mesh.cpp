#include "render/mesh.h"

#include <utility>

#include "render/gl.h"

namespace s3d {

Mesh::~Mesh() { destroy(); }

Mesh::Mesh(Mesh&& other) noexcept { *this = std::move(other); }

Mesh& Mesh::operator=(Mesh&& other) noexcept {
  if (this != &other) {
    destroy();
    vao_ = other.vao_;
    vbo_ = other.vbo_;
    ibo_ = other.ibo_;
    vertex_count_ = other.vertex_count_;
    index_count_ = other.index_count_;
    other.vao_ = other.vbo_ = other.ibo_ = 0;
    other.vertex_count_ = other.index_count_ = 0;
  }
  return *this;
}

void Mesh::upload(const std::vector<Vertex>& vertices) {
  upload(vertices, {});
}

void Mesh::upload(const std::vector<Vertex>& vertices,
                  const std::vector<uint32_t>& indices) {
  destroy();
  vertex_count_ = vertices.size();
  index_count_ = indices.size();
  if (vertex_count_ == 0) return;

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);

  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(), GL_STATIC_DRAW);

  const GLsizei stride = sizeof(Vertex);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(Vertex, color)));
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(Vertex, uv)));

  if (index_count_ > 0) {
    glGenBuffers(1, &ibo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                 indices.data(), GL_STATIC_DRAW);
  }

  glBindVertexArray(0);
}

void Mesh::destroy() {
  if (ibo_ != 0) {
    glDeleteBuffers(1, &ibo_);
    ibo_ = 0;
  }
  if (vbo_ != 0) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_ != 0) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  vertex_count_ = 0;
  index_count_ = 0;
}

void Mesh::draw() const {
  if (vertex_count_ == 0) return;
  glBindVertexArray(vao_);
  if (index_count_ > 0) {
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index_count_),
                   GL_UNSIGNED_INT, nullptr);
  } else {
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertex_count_));
  }
  glBindVertexArray(0);
}

void Mesh::draw_lines() const {
  if (vertex_count_ == 0) return;
  glBindVertexArray(vao_);
  glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertex_count_));
  glBindVertexArray(0);
}

}  // namespace s3d
