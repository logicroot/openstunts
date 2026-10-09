#pragma once
// Cross-platform window + OpenGL context creation (SDL2).

#include <SDL2/SDL.h>

#include <string>

namespace s3d {

struct WindowConfig {
  std::string title = "Stunts3D";
  int width = 1280;
  int height = 720;
  bool vsync = true;
  bool fullscreen = false;
  bool resizable = true;
  bool highdpi = true;
  int gl_major = 3;
  int gl_minor = 3;
};

// Owns the SDL window and the GL context. SDL video subsystem is initialized on
// create() and shut down on destroy().
class Window {
 public:
  Window() = default;
  ~Window();

  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  bool create(const WindowConfig& cfg);
  void destroy();

  void swap();

  SDL_Window* handle() const { return window_; }
  SDL_GLContext gl_context() const { return gl_; }

  int width() const;
  int height() const;
  int drawable_width() const;
  int drawable_height() const;
  float aspect() const;

 private:
  SDL_Window* window_ = nullptr;
  SDL_GLContext gl_ = nullptr;
};

}  // namespace s3d
