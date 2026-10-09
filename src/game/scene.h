#pragma once
// Scene interface: menus, debug views and races are all scenes.

#include <SDL2/SDL.h>

#include <string>

namespace s3d {

class Renderer;
struct Controls;

class Scene {
 public:
  virtual ~Scene() = default;

  // Called once after creation. Return false to abort.
  virtual bool on_enter() { return true; }
  virtual void on_exit() {}

  // Fixed-step simulation update.
  virtual void update(double dt, const Controls& controls) = 0;

  // Render the frame (renderer already began the frame).
  virtual void render(Renderer& renderer, int width, int height) = 0;

  virtual void handle_event(const SDL_Event&) {}

  // If non-empty, the application switches to this scene's name next frame.
  virtual std::string next_scene() const { return {}; }
};

}  // namespace s3d
