#pragma once
// Screen-space HUD text drawn from a BitmapFont.
//
// Text is built as one quad per set font pixel in *pixel* coordinates with the
// origin at the top-left and y growing downward. The HUD is drawn after the
// world with depth testing off, so it always sits on top.

#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "assets/font.h"
#include "render/mesh.h"
#include "render/shader.h"

namespace s3d {

// Appends the quads for `text` to `out`. (x, y) is the top-left of the first
// glyph cell; each font pixel becomes a `scale`x`scale` pixel quad. Returns the
// x coordinate just past the last glyph.
float append_text(std::vector<Vertex>& out, const BitmapFont& font,
                  const std::string& text, float x, float y, float scale,
                  const glm::vec4& color);

class HudRenderer {
 public:
  bool init();
  void shutdown();

  // verts are in pixels; origin top-left. Depth testing is disabled for the
  // draw, so call this last.
  void draw(const std::vector<Vertex>& verts, int drawable_width,
            int drawable_height);

 private:
  Shader shader_;
  Mesh mesh_;
};

}  // namespace s3d
