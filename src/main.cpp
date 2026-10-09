#include <SDL2/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "assets/trk.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/window.h"
#include "render/camera.h"
#include "render/gl.h"
#include "render/renderer.h"
#include "render/screenshot.h"
#include "world/track_mesh.h"
#include "world/world_units.h"

using namespace s3d;

namespace {

void aim_at(Camera& cam, const glm::vec3& target) {
  const glm::vec3 d = glm::normalize(target - cam.position);
  cam.yaw_deg = glm::degrees(std::atan2(d.z, d.x));
  cam.pitch_deg = glm::degrees(std::asin(d.y));
}

}  // namespace

int main(int argc, char** argv) {
  std::string assets_dir;
  std::string track_file;
  std::string screenshot_path;
  int screenshot_frame = 0;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--assets" && i + 1 < argc) {
      assets_dir = argv[++i];
    } else if (a == "--track" && i + 1 < argc) {
      track_file = argv[++i];
    } else if (a == "--screenshot" && i + 1 < argc) {
      screenshot_path = argv[++i];
      if (screenshot_frame == 0) screenshot_frame = 3;
    } else if (a == "--frames" && i + 1 < argc) {
      screenshot_frame = std::atoi(argv[++i]);
    } else {
      S3D_WARN("unknown argument: %s", a.c_str());
    }
  }

  if (assets_dir.empty()) assets_dir = find_asset_dir();
  if (track_file.empty()) track_file = path_join(assets_dir, "DEFAULT.TRK");

  Track track = Track::load(track_file);
  S3D_INFO("track: %s skybox=%s", track_file.c_str(), track.skybox_name());

  Window window;
  WindowConfig cfg;
  cfg.title = "Stunts3D — " + track_file;
  if (!window.create(cfg)) return 1;
  if (!gl::load()) {
    S3D_ERROR("failed to load required OpenGL 3.3 functions");
    return 1;
  }
  S3D_INFO("GL version : %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  S3D_INFO("GL renderer: %s", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

  Renderer renderer;
  if (!renderer.init()) {
    S3D_ERROR("renderer init failed");
    return 1;
  }
  renderer.set_fog(glm::vec3(0.60f, 0.72f, 0.85f), 1.0f / 60000.0f);

  const Mesh track_mesh = build_track_mesh(track);

  Camera camera;
  const float cx = world::cell_center_x(15);
  const float cz = world::cell_center_z(15);
  camera.position = glm::vec3(cx - 9000.0f, 6500.0f, cz - 12000.0f);
  aim_at(camera, glm::vec3(cx, 0.0f, cz));

  bool running = true;
  bool mouse_look = false;
  int frame_index = 0;
  int hotkey_shot = 0;
  Uint32 last_ticks = SDL_GetTicks();
  float fps_timer = 0.0f;
  int frames = 0;

  while (running) {
    const Uint32 now = SDL_GetTicks();
    float dt = static_cast<float>(now - last_ticks) / 1000.0f;
    last_ticks = now;
    if (dt > 0.1f) dt = 0.1f;

    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
      if (e.type == SDL_QUIT) running = false;
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
        if (e.key.keysym.sym == SDLK_F12) {
          char p[64];
          std::snprintf(p, sizeof(p), "stunts3d_%03d.png", hotkey_shot++);
          capture_frame_png(p, window.drawable_width(), window.drawable_height());
        }
        if (e.key.keysym.sym == SDLK_F1) {
          mouse_look = !mouse_look;
          SDL_SetRelativeMouseMode(mouse_look ? SDL_TRUE : SDL_FALSE);
        }
      }
      if (e.type == SDL_MOUSEMOTION && mouse_look) {
        camera.add_mouse(static_cast<float>(e.motion.xrel), static_cast<float>(e.motion.yrel));
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
        mouse_look = true;
        SDL_SetRelativeMouseMode(SDL_TRUE);
      }
      if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
        mouse_look = false;
        SDL_SetRelativeMouseMode(SDL_FALSE);
      }
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float speed = 4000.0f * dt;
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) speed *= 4.0f;
    const glm::vec3 fwd = camera.forward();
    const glm::vec3 right = camera.right();
    if (keys[SDL_SCANCODE_W]) camera.move(fwd * speed);
    if (keys[SDL_SCANCODE_S]) camera.move(-fwd * speed);
    if (keys[SDL_SCANCODE_D]) camera.move(right * speed);
    if (keys[SDL_SCANCODE_A]) camera.move(-right * speed);
    if (keys[SDL_SCANCODE_E]) camera.move(glm::vec3(0.0f, speed, 0.0f));
    if (keys[SDL_SCANCODE_Q]) camera.move(glm::vec3(0.0f, -speed, 0.0f));

    renderer.begin_frame(window.drawable_width(), window.drawable_height(),
                         glm::vec3(0.55f, 0.72f, 0.92f));
    renderer.draw_world(track_mesh, camera);

    ++frame_index;
    if (!screenshot_path.empty() && frame_index >= screenshot_frame) {
      capture_frame_png(screenshot_path, window.drawable_width(), window.drawable_height());
      running = false;
    }
    window.swap();

    ++frames;
    fps_timer += dt;
    if (fps_timer >= 1.0f) {
      char title[128];
      std::snprintf(title, sizeof(title), "Stunts3D — %s — %d fps", track_file.c_str(),
                    frames);
      SDL_SetWindowTitle(window.handle(), title);
      frames = 0;
      fps_timer = 0.0f;
    }
  }

  renderer.shutdown();
  window.destroy();
  SDL_Quit();
  return 0;
}
