#include "render/renderer.h"

#include "core/log.h"
#include "render/gl.h"

namespace s3d {

namespace {

const char* kWorldVS = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_world_pos;
out vec3 v_normal;
out vec4 v_color;
out vec2 v_uv;
out float v_view_depth;

void main() {
  vec4 world = u_model * vec4(a_pos, 1.0);
  vec4 view_pos = u_view * world;
  v_world_pos = world.xyz;
  v_normal = mat3(u_model) * a_normal;
  v_color = a_color;
  v_uv = a_uv;
  v_view_depth = -view_pos.z;
  gl_Position = u_proj * view_pos;
}
)GLSL";

const char* kWorldFS = R"GLSL(
#version 330 core
in vec3 v_world_pos;
in vec3 v_normal;
in vec4 v_color;
in vec2 v_uv;
in float v_view_depth;

uniform vec3 u_light_dir;
uniform vec4 u_fog_color;
uniform float u_fog_density;

out vec4 frag_color;

void main() {
  vec3 n = normalize(v_normal);
  float ndl = max(dot(n, normalize(u_light_dir)), 0.0);
  // Hemisphere-ish ambient for a softer, modern look.
  float ambient = mix(0.35, 0.65, n.y * 0.5 + 0.5);
  vec3 lit = v_color.rgb * (ambient + 0.85 * ndl);

  vec3 fog = u_fog_color.rgb;
  float fog_amount = 1.0 - exp(-u_fog_density * v_view_depth);
  frag_color = vec4(mix(lit, fog, clamp(fog_amount, 0.0, 1.0)), v_color.a);
}
)GLSL";

}  // namespace

bool Renderer::init() {
  if (!world_shader_.build(kWorldVS, kWorldFS, "world")) return false;
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  return true;
}

void Renderer::shutdown() { world_shader_.destroy(); }

void Renderer::begin_frame(int drawable_width, int drawable_height,
                           const glm::vec3& clear_color) {
  current_aspect_ = (drawable_height > 0)
                        ? static_cast<float>(drawable_width) / static_cast<float>(drawable_height)
                        : 1.0f;
  glViewport(0, 0, drawable_width, drawable_height);
  glClearColor(clear_color.r, clear_color.g, clear_color.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
}

void Renderer::draw_world(const Mesh& mesh, const Camera& camera,
                          const glm::mat4& model) {
  if (mesh.empty()) return;
  // The HUD/sky passes disable depth testing, so re-enable it here to keep the
  // draw order-independent.
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  world_shader_.use();
  world_shader_.set_mat4("u_model", model);
  world_shader_.set_mat4("u_view", camera.view());
  world_shader_.set_mat4("u_proj", camera.projection(current_aspect_));
  world_shader_.set_vec3("u_light_dir", light_dir_);
  world_shader_.set_vec4("u_fog_color", glm::vec4(fog_color_, 1.0f));
  world_shader_.set_float("u_fog_density", fog_density_);
  mesh.draw();
}

void Renderer::draw_world_lines(const Mesh& mesh, const Camera& camera,
                                const glm::mat4& model) {
  if (mesh.empty()) return;
  world_shader_.use();
  world_shader_.set_mat4("u_model", model);
  world_shader_.set_mat4("u_view", camera.view());
  world_shader_.set_mat4("u_proj", camera.projection(current_aspect_));
  world_shader_.set_vec3("u_light_dir", light_dir_);
  world_shader_.set_vec4("u_fog_color", glm::vec4(fog_color_, 1.0f));
  world_shader_.set_float("u_fog_density", fog_density_);
  mesh.draw_lines();
}

}  // namespace s3d
