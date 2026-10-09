#pragma once
// GLSL program wrapper.

#include <glm/glm.hpp>

#include <string>

namespace s3d {

class Shader {
 public:
  Shader() = default;
  ~Shader();

  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  // Compiles + links. On failure logs the GLSL errors and returns false.
  bool build(const char* vertex_src, const char* fragment_src, const std::string& name);

  void destroy();
  void use() const;

  unsigned program() const { return program_; }

  void set_int(const char* name, int v) const;
  void set_float(const char* name, float v) const;
  void set_vec2(const char* name, const glm::vec2& v) const;
  void set_vec3(const char* name, const glm::vec3& v) const;
  void set_vec4(const char* name, const glm::vec4& v) const;
  void set_mat3(const char* name, const glm::mat3& m) const;
  void set_mat4(const char* name, const glm::mat4& m) const;

 private:
  int location(const char* name) const;

  unsigned program_ = 0;
  std::string name_;
};

}  // namespace s3d
