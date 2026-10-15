#pragma once
// Application framework: owns the window, renderer, input and scene loop.

#include <memory>
#include <string>
#include <functional>
#include <map>

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

  // Registers a scene factory under `name`. When the active scene's
  // next_scene() returns this name, the application constructs the factory's
  // scene and enters it. The special name "quit" stops the loop.
  using SceneFactory = std::function<std::unique_ptr<Scene>()>;
  void register_scene(const std::string& name, SceneFactory factory);

  // Advance the simulation by a fixed step per rendered frame instead of by the
  // wall clock, making an offline capture reproducible. Used by the screenshot
  // path. Must be set before run().
  void set_deterministic(bool on) { deterministic_ = on; }

 private:
  Config config_;
  std::string asset_dir_;
  Window window_;
  Renderer renderer_;
  Input input_;
  FixedTimestep timestep_{60.0};
  std::unique_ptr<Scene> scene_;
  std::map<std::string, SceneFactory> scene_factories_;
  bool running_ = false;
  bool deterministic_ = false;
};

}  // namespace s3d
