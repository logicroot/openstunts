#include "game/application.h"

#include <cstdio>

#include "core/log.h"
#include "game/scene.h"
#include "render/gl.h"
#include "render/screenshot.h"

namespace s3d {

Application::~Application() {
  if (scene_) scene_->on_exit();
  renderer_.shutdown();
  window_.destroy();
}

bool Application::init(const Config& config, const std::string& asset_dir) {
  config_ = config;
  asset_dir_ = asset_dir;

  WindowConfig wc;
  wc.width = config.width;
  wc.height = config.height;
  wc.vsync = config.vsync;
  wc.fullscreen = config.fullscreen;
  wc.highdpi = config.highdpi;
  if (!window_.create(wc)) return false;

  if (!gl::load()) {
    S3D_ERROR("failed to load required OpenGL 3.3 functions");
    return false;
  }
  S3D_INFO("GL version : %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  S3D_INFO("GL renderer: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

  if (!renderer_.init()) {
    S3D_ERROR("renderer initialization failed");
    return false;
  }
  renderer_.set_fog(glm::vec3(0.60f, 0.72f, 0.85f), 1.0f / 60000.0f);
  renderer_.set_pattern_fill(config.pattern_fill);

  SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
  for (int i = 0; i < SDL_NumJoysticks(); ++i) {
    if (SDL_IsGameController(i)) {
      SDL_GameControllerOpen(i);  // kept open for its lifetime
      break;
    }
  }
  return true;
}

void Application::set_scene(std::unique_ptr<Scene> scene) {
  if (scene_) scene_->on_exit();
  scene_ = std::move(scene);
  if (scene_) scene_->on_enter();
}

void Application::register_scene(const std::string& name, SceneFactory factory) {
  scene_factories_[name] = std::move(factory);
}

void Application::run() {
  running_ = true;
  Uint32 last_ticks = SDL_GetTicks();
  // Deterministic mode advances by exactly one fixed step per rendered frame
  // instead of by the wall clock. Rendered frame N then always corresponds to
  // the same simulated time, so an offline capture is reproducible regardless
  // of how fast the machine renders. (In real-time mode a frame that takes
  // longer than a step runs catch-up steps, so the number of steps behind any
  // given rendered frame drifts with load.)
  const double fixed_step = timestep_.step_seconds();
  float fps_timer = 0.0f;
  int frames = 0;

  while (running_) {
    const Uint32 now = SDL_GetTicks();
    const double wall_seconds = static_cast<double>(now - last_ticks) / 1000.0;
    last_ticks = now;
    const double frame_seconds = deterministic_ ? fixed_step : wall_seconds;

    // Mouse coordinates arrive in window pixels; scale them to the drawable
    // (framebuffer) pixels the GUI and the scenes work in.
    {
      const int lw = window_.width();
      const int lh = window_.height();
      const float msx = (lw > 0) ? static_cast<float>(window_.drawable_width()) / static_cast<float>(lw) : 1.0f;
      const float msy = (lh > 0) ? static_cast<float>(window_.drawable_height()) / static_cast<float>(lh) : 1.0f;
      input_.set_mouse_scale(msx, msy);
    }
    input_.begin_frame();
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
      input_.handle_event(e);
      if (scene_) scene_->handle_event(e);
    }
    if (input_.quit_requested()) break;

    const int steps = timestep_.advance(frame_seconds);
    for (int i = 0; i < steps && scene_; ++i) {
      scene_->update(timestep_.step_seconds(), input_.get());
      input_.consume_edges();
    }

    const int w = window_.drawable_width();
    const int h = window_.drawable_height();
    renderer_.begin_frame(w, h, glm::vec3(0.55f, 0.72f, 0.92f));
    if (scene_) scene_->render(renderer_, w, h);
    window_.swap();

    if (!scene_) break;
    const std::string next = scene_->next_scene();
    if (!next.empty()) {
      if (next == "quit") break;
      auto it = scene_factories_.find(next);
      if (it == scene_factories_.end()) {
        S3D_WARN("no scene registered as '%s'", next.c_str());
        break;
      }
      set_scene(it->second());
      if (!scene_) break;
    }

    ++frames;
    fps_timer += static_cast<float>(wall_seconds);
    if (fps_timer >= 1.0f) {
      char title[160];
      std::snprintf(title, sizeof(title), "OpenStunts — %d fps", frames);
      SDL_SetWindowTitle(window_.handle(), title);
      frames = 0;
      fps_timer = 0.0f;
    }
  }
}

}  // namespace s3d
