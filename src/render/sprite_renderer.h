#pragma once
// Screen-space textured quads in pixel coordinates (origin top-left, y down).
//
// Used for the skybox horizon panorama, whose scenery layers are palette
// bitmaps. Like the HUD renderer this disables depth testing, but it samples an
// RGBA texture instead of using a flat vertex colour.

#include <vector>

#include <glm/glm.hpp>

#include "render/mesh.h"
#include "render/shader.h"
#include "render/texture.h"

namespace s3d {

class SpriteRenderer {
 public:
  bool init();
  void shutdown();

  // `verts` are triangles in pixels; the quad's uv range selects the region of
  // `texture` to sample. Blending is disabled by default (the layers are opaque,
  // matching seg012.asm sprite_putimage_and_alt). Pass `blend` for the cockpit's
  // AND/OR overlay blits (the "inm"/"ins" pairs, the digital glyphs and "dast"),
  // whose transparent texels must leave the destination untouched.
  void draw(const Texture& texture, const std::vector<Vertex>& verts,
            int drawable_width, int drawable_height, bool blend = false);

 private:
  Shader shader_;
  Mesh mesh_;
};

}  // namespace s3d
