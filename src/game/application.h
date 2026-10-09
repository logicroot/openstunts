#pragma once
// Application framework: owns the window, renderer, input and scene loop.

#include <memory>
#include <string>

#include "core/config.h"
#include "core/fixed_timestep.h"
#include "platform/input.h"
#include "platform/window.h"
#include "render/renderer.h"

namespace s3d {

class Scene;

class Application {
 public:
  Application() = default;
  ~Application();

  // Creates the window/renderer and enters the initial scene.
  bool init(const Config& config, const std::string& asset_dir);
  void run();
  void quit() { running_ = false; }

  Renderer& renderer() { return renderer_; }
  Window& window() { return window_; }
  Input& input() { return input_; }
  const Config& config() const { return config_; }
  const std::string& asset_dir() const { return asset_dir_; }

  void set_scene(std::unique_ptr<Scene> scene);

 private:
  Config config_;
  std::string asset_dir_;
  Window window_;
  Renderer renderer_;
  Input input_;
  FixedTimestep timestep_{60.0};
  std::unique_ptr<Scene> scene_;
  bool running_ = false;
};

}  // namespace s3d
