#pragma once
// Forward renderer: draws world meshes with a single lit shader.

#include <glm/glm.hpp>

#include "render/camera.h"
#include "render/mesh.h"
#include "render/shader.h"

namespace s3d {

class Renderer {
 public:
  bool init();
  void shutdown();

  void begin_frame(int drawable_width, int drawable_height, const glm::vec3& clear_color);
  void draw_world(const Mesh& mesh, const Camera& camera,
                  const glm::mat4& model = glm::mat4(1.0f));

  // Biases the depth of subsequently drawn polygons (glPolygonOffset). Used to
  // push layered, nearly coplanar track surfaces (ground base, terrain relief,
  // elements, decals) behind one another so their draw order is authoritative
  // and they do not z-fight as the camera moves. (0, 0) disables the bias.
  void set_polygon_offset(float factor, float units);

  // Draws a mesh built as vertex pairs with GL_LINES (used for the flashing
  // diner sign). Same lighting/colour shader as draw_world.
  void draw_world_lines(const Mesh& mesh, const Camera& camera,
                        const glm::mat4& model = glm::mat4(1.0f));

  void set_light_direction(const glm::vec3& dir) { light_dir_ = glm::normalize(dir); }
  void set_fog(const glm::vec3& color, float density) {
    fog_color_ = color;
    fog_density_ = density;
  }

  // Enables the original's screen-space dither for patterned materials (elevated
  // and sloped road surfaces, the windmill, ...). On by default; disabled via the
  // `pattern_fill` config key. Flat materials are unaffected either way.
  void set_pattern_fill(bool on) { pattern_fill_ = on; }

  Shader& world_shader() { return world_shader_; }

 private:
  void set_pattern_uniforms();

  Shader world_shader_;
  // Direction towards the sun, in world space. The racing view looks down +Z
  // from the grid, so the sun sits behind and to the right of it (a negative Z
  // here) to keep the start/finish gate and the cars in front of it lit rather
  // than back-lit. See the world fragment shader for how softly it is applied.
  glm::vec3 light_dir_{0.45f, 1.0f, -0.40f};
  glm::vec3 fog_color_{0.55f, 0.70f, 0.85f};
  float fog_density_ = 0.0f;
  float current_aspect_ = 1.0f;
  bool pattern_fill_ = true;
  // Maps a window pixel to the original 320x200 pixel grid; set in begin_frame.
  glm::vec2 pattern_scale_{1.0f};
};

}  // namespace s3d
