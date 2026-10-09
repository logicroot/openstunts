#include "game/debug_track_scene.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>

#include "assets/shape3d.h"
#include "core/log.h"
#include "core/paths.h"
#include "render/renderer.h"
#include "render/screenshot.h"
#include "world/track_mesh.h"
#include "world/track_shapes.h"
#include "world/world_units.h"

namespace s3d {

namespace {

void aim_at(Camera& cam, const glm::vec3& target) {
  const glm::vec3 d = glm::normalize(target - cam.position);
  cam.yaw_deg = glm::degrees(std::atan2(d.z, d.x));
  cam.pitch_deg = glm::degrees(std::asin(d.y));
}

}  // namespace

DebugTrackScene::DebugTrackScene(Track track, std::string name, std::string asset_dir)
    : track_(std::move(track)), name_(std::move(name)), asset_dir_(std::move(asset_dir)) {}

bool DebugTrackScene::on_enter() {
  ShapeLibrary lib;
  if (!asset_dir_.empty()) {
    lib.load_p3s(path_join(asset_dir_, "GAME1.P3S"));
    lib.load_p3s(path_join(asset_dir_, "GAME2.P3S"));
  }
  if (!lib.shapes().empty()) {
    world::TrackShapeStats stats;
    mesh_ = world::build_track_shape_mesh(track_, lib, &stats);
    S3D_INFO("track shapes: ground=%d terrain=%d elements=%d overlays=%d missing=%d",
             stats.ground_cells, stats.terrain_shapes, stats.elements, stats.overlays,
             stats.missing_shapes);
  } else {
    S3D_WARN("no GAME1/GAME2 shapes found; falling back to placeholder geometry");
    mesh_ = build_track_mesh(track_);
  }

  const float cx = world::cell_center_x(world::kTrackSize / 2);
  const float cz = world::cell_center_z(world::kTrackSize / 2);
  if (top_down_) {
    camera_.position = glm::vec3(cx, 18000.0f, cz + 3000.0f);
  } else {
    camera_.position = glm::vec3(cx - 9000.0f, 6500.0f, cz - 12000.0f);
  }
  aim_at(camera_, glm::vec3(cx, 0.0f, cz));
  S3D_INFO("debug scene: %s", name_.c_str());
  return true;
}

void DebugTrackScene::update(double dt, const Controls& controls) {
  (void)controls;
  float speed = 6000.0f * static_cast<float>(dt);
  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT]) speed *= 4.0f;

  const glm::vec3 fwd = camera_.forward();
  const glm::vec3 right = camera_.right();
  if (keys[SDL_SCANCODE_W]) camera_.move(fwd * speed);
  if (keys[SDL_SCANCODE_S]) camera_.move(-fwd * speed);
  if (keys[SDL_SCANCODE_D]) camera_.move(right * speed);
  if (keys[SDL_SCANCODE_A]) camera_.move(-right * speed);
  if (keys[SDL_SCANCODE_E]) camera_.move(glm::vec3(0.0f, speed, 0.0f));
  if (keys[SDL_SCANCODE_Q]) camera_.move(glm::vec3(0.0f, -speed, 0.0f));
}

void DebugTrackScene::render(Renderer& renderer, int width, int height) {
  renderer.draw_world(mesh_, camera_);

  ++frame_index_;
  if (pending_hotkey_) {
    pending_hotkey_ = false;
    char p[64];
    std::snprintf(p, sizeof(p), "stunts3d_%03d.png", hotkey_shots_++);
    capture_frame_png(p, width, height);
  }
  if (!screenshot_path_.empty() && frame_index_ >= screenshot_frame_) {
    capture_frame_png(screenshot_path_, width, height);
    screenshot_path_.clear();
    finished_ = true;
  }
}

void DebugTrackScene::handle_event(const SDL_Event& e) {
  if (e.type == SDL_KEYDOWN) {
    if (e.key.keysym.sym == SDLK_F12) pending_hotkey_ = true;
    if (e.key.keysym.sym == SDLK_t) {
      top_down_ = !top_down_;
      const float cx = world::cell_center_x(world::kTrackSize / 2);
      const float cz = world::cell_center_z(world::kTrackSize / 2);
      camera_.position = top_down_ ? glm::vec3(cx, 18000.0f, cz + 3000.0f)
                                   : glm::vec3(cx - 9000.0f, 6500.0f, cz - 12000.0f);
      aim_at(camera_, glm::vec3(cx, 0.0f, cz));
    }
    if (e.key.keysym.sym == SDLK_F1) {
      mouse_look_ = !mouse_look_;
      SDL_SetRelativeMouseMode(mouse_look_ ? SDL_TRUE : SDL_FALSE);
    }
  }
  if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT) {
    mouse_look_ = true;
    SDL_SetRelativeMouseMode(SDL_TRUE);
  }
  if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_RIGHT) {
    mouse_look_ = false;
    SDL_SetRelativeMouseMode(SDL_FALSE);
  }
  if (e.type == SDL_MOUSEMOTION && mouse_look_) {
    camera_.add_mouse(static_cast<float>(e.motion.xrel), static_cast<float>(e.motion.yrel));
  }
}

void DebugTrackScene::request_screenshot(std::string path, int frames) {
  screenshot_path_ = std::move(path);
  screenshot_frame_ = frames > 0 ? frames : 1;
}

}  // namespace s3d
