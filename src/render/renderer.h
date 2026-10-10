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

  // Draws a mesh built as vertex pairs with GL_LINES (used for the flashing
  // diner sign). Same lighting/colour shader as draw_world.
  void draw_world_lines(const Mesh& mesh, const Camera& camera,
                        const glm::mat4& model = glm::mat4(1.0f));

  void set_light_direction(const glm::vec3& dir) { light_dir_ = glm::normalize(dir); }
  void set_fog(const glm::vec3& color, float density) {
    fog_color_ = color;
    fog_density_ = density;
  }

  Shader& world_shader() { return world_shader_; }

 private:
  Shader world_shader_;
  glm::vec3 light_dir_{0.4f, 1.0f, 0.3f};
  glm::vec3 fog_color_{0.55f, 0.70f, 0.85f};
  float fog_density_ = 0.0f;
  float current_aspect_ = 1.0f;
};

}  // namespace s3d
