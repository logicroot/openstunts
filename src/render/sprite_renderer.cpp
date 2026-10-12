#include "render/sprite_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "render/gl.h"

namespace s3d {

namespace {

const char* kSpriteVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_ortho;

out vec2 v_uv;

void main() {
  v_uv = a_uv;
  gl_Position = u_ortho * vec4(a_pos, 1.0);
}
)GLSL";

const char* kSpriteFS = R"GLSL(
#version 330 core
in vec2 v_uv;
out vec4 frag_color;

uniform sampler2D u_tex;

void main() { frag_color = texture(u_tex, v_uv); }
)GLSL";

}  // namespace

bool SpriteRenderer::init() { return shader_.build(kSpriteVS, kSpriteFS, "sprite"); }

void SpriteRenderer::shutdown() {
  shader_.destroy();
  mesh_.destroy();
}

void SpriteRenderer::draw(const Texture& texture, const std::vector<Vertex>& verts,
                          int drawable_width, int drawable_height) {
  if (verts.empty() || !texture.valid() || drawable_width <= 0 ||
      drawable_height <= 0) {
    return;
  }
  mesh_.upload(verts);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  shader_.use();
  const glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(drawable_width),
                                     static_cast<float>(drawable_height), 0.0f,
                                     -1.0f, 1.0f);
  shader_.set_mat4("u_ortho", ortho);
  texture.bind(0);
  shader_.set_int("u_tex", 0);
  mesh_.draw();
}

}  // namespace s3d
