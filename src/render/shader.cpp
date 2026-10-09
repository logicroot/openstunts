#include "render/shader.h"

#include <vector>

#include "core/log.h"
#include "render/gl.h"

namespace s3d {

namespace {

unsigned compile_stage(GLenum type, const char* src, const std::string& name) {
  const GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (ok != GL_TRUE) {
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
    glGetShaderInfoLog(shader, len, nullptr, log.data());
    S3D_ERROR("shader '%s': %s stage failed to compile:\n%s", name.c_str(),
              type == GL_VERTEX_SHADER ? "vertex" : "fragment", log.data());
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

Shader::~Shader() { destroy(); }

bool Shader::build(const char* vertex_src, const char* fragment_src,
                   const std::string& name) {
  destroy();
  name_ = name;
  const unsigned vs = compile_stage(GL_VERTEX_SHADER, vertex_src, name);
  if (vs == 0) return false;
  const unsigned fs = compile_stage(GL_FRAGMENT_SHADER, fragment_src, name);
  if (fs == 0) {
    glDeleteShader(vs);
    return false;
  }

  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint linked = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    GLint len = 0;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &len);
    std::vector<char> log(static_cast<size_t>(len > 1 ? len : 1));
    glGetProgramInfoLog(program_, len, nullptr, log.data());
    S3D_ERROR("shader '%s': link failed:\n%s", name.c_str(), log.data());
    glDeleteProgram(program_);
    program_ = 0;
    return false;
  }
  return true;
}

void Shader::destroy() {
  if (program_ != 0) {
    glDeleteProgram(program_);
    program_ = 0;
  }
}

void Shader::use() const { glUseProgram(program_); }

int Shader::location(const char* name) const {
  return glGetUniformLocation(program_, name);
}

void Shader::set_int(const char* name, int v) const {
  glUniform1i(location(name), v);
}
void Shader::set_float(const char* name, float v) const {
  glUniform1f(location(name), v);
}
void Shader::set_vec2(const char* name, const glm::vec2& v) const {
  glUniform2f(location(name), v.x, v.y);
}
void Shader::set_vec3(const char* name, const glm::vec3& v) const {
  glUniform3f(location(name), v.x, v.y, v.z);
}
void Shader::set_vec4(const char* name, const glm::vec4& v) const {
  glUniform4f(location(name), v.x, v.y, v.z, v.w);
}
void Shader::set_mat3(const char* name, const glm::mat3& m) const {
  glUniformMatrix3fv(location(name), 1, GL_FALSE, &m[0][0]);
}
void Shader::set_mat4(const char* name, const glm::mat4& m) const {
  glUniformMatrix4fv(location(name), 1, GL_FALSE, &m[0][0]);
}

}  // namespace s3d
