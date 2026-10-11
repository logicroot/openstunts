#include "render/hud.h"

#include <glm/gtc/matrix_transform.hpp>

#include "render/gl.h"

namespace s3d {

namespace {

const char* kHudVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 2) in vec4 a_color;

uniform mat4 u_ortho;

out vec4 v_color;

void main() {
  v_color = a_color;
  gl_Position = u_ortho * vec4(a_pos, 1.0);
}
)GLSL";

const char* kHudFS = R"GLSL(
#version 330 core
in vec4 v_color;
out vec4 frag_color;

void main() { frag_color = v_color; }
)GLSL";

}  // namespace

float append_text(std::vector<Vertex>& out, const BitmapFont& font,
                  const std::string& text, float x, float y, float scale,
                  const glm::vec4& color) {
  const int rows = font.rows();
  float pen = x;
  for (unsigned char ch : text) {
    const FontGlyph& g = font.glyph(ch);
    const int bpr = g.columns > 0 ? g.columns / 8 : 0;
    if (bpr > 0) {
      Vertex v;
      v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
      v.color = color;
      for (int r = 0; r < rows; ++r) {
        for (int byte = 0; byte < bpr; ++byte) {
          const uint8_t bits = g.bits[static_cast<size_t>(r) * bpr + byte];
          if (bits == 0) continue;
          for (int bit = 0; bit < 8; ++bit) {
            if (((bits >> (7 - bit)) & 1) == 0) continue;
            const float px = pen + static_cast<float>(byte * 8 + bit) * scale;
            const float py = y + static_cast<float>(r) * scale;
            const float sx = px + scale;
            const float sy = py + scale;
            v.position = {px, py, 0.0f};
            out.push_back(v);
            v.position = {sx, py, 0.0f};
            out.push_back(v);
            v.position = {sx, sy, 0.0f};
            out.push_back(v);
            v.position = {px, py, 0.0f};
            out.push_back(v);
            v.position = {sx, sy, 0.0f};
            out.push_back(v);
            v.position = {px, sy, 0.0f};
            out.push_back(v);
          }
        }
      }
    }
    pen += static_cast<float>(g.advance) * scale;
  }
  return pen;
}

bool HudRenderer::init() { return shader_.build(kHudVS, kHudFS, "hud"); }

void HudRenderer::shutdown() {
  shader_.destroy();
  mesh_.destroy();
}

void HudRenderer::draw(const std::vector<Vertex>& verts, int drawable_width,
                       int drawable_height) {
  if (verts.empty() || drawable_width <= 0 || drawable_height <= 0) return;
  mesh_.upload(verts);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  shader_.use();
  const glm::mat4 ortho = glm::ortho(0.0f, static_cast<float>(drawable_width),
                                     static_cast<float>(drawable_height), 0.0f,
                                     -1.0f, 1.0f);
  shader_.set_mat4("u_ortho", ortho);
  mesh_.draw();
}

}  // namespace s3d
