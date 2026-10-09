#include "platform/window.h"

#include "core/log.h"

namespace s3d {

Window::~Window() { destroy(); }

bool Window::create(const WindowConfig& cfg) {
  if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
      S3D_ERROR("SDL_InitSubSystem(VIDEO) failed: %s", SDL_GetError());
      return false;
    }
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, cfg.gl_major);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, cfg.gl_minor);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#ifdef __APPLE__
  // macOS only exposes 3.2+ core contexts with forward compatibility.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG, SDL_TRUE);
#endif
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, SDL_TRUE);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

  Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
  if (cfg.resizable) flags |= SDL_WINDOW_RESIZABLE;
  if (cfg.fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  if (cfg.highdpi) flags |= SDL_WINDOW_ALLOW_HIGHDPI;

  int w = cfg.width;
  int h = cfg.height;
  window_ = SDL_CreateWindow(cfg.title.c_str(), SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, w, h, flags);
  if (window_ == nullptr) {
    S3D_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
    return false;
  }

  gl_ = SDL_GL_CreateContext(window_);
  if (gl_ == nullptr) {
    S3D_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
    destroy();
    return false;
  }

  SDL_GL_SetSwapInterval(cfg.vsync ? 1 : 0);
  return true;
}

void Window::destroy() {
  if (gl_ != nullptr) {
    SDL_GL_DeleteContext(gl_);
    gl_ = nullptr;
  }
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
}

void Window::swap() {
  if (window_ != nullptr) SDL_GL_SwapWindow(window_);
}

int Window::width() const {
  int w = 0, h = 0;
  if (window_ != nullptr) SDL_GetWindowSize(window_, &w, &h);
  return w;
}

int Window::height() const {
  int w = 0, h = 0;
  if (window_ != nullptr) SDL_GetWindowSize(window_, &w, &h);
  return h;
}

int Window::drawable_width() const {
  int w = 0, h = 0;
  if (window_ != nullptr) SDL_GL_GetDrawableSize(window_, &w, &h);
  return w;
}

int Window::drawable_height() const {
  int w = 0, h = 0;
  if (window_ != nullptr) SDL_GL_GetDrawableSize(window_, &w, &h);
  return h;
}

float Window::aspect() const {
  const int w = drawable_width();
  const int h = drawable_height();
  return (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
}

}  // namespace s3d
