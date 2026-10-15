#include "game/debug_track_scene.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>

#include "assets/palette.h"
#include "assets/shape3d.h"
#include "core/log.h"
#include "core/paths.h"
#include "platform/input.h"
#include "render/renderer.h"
#include "render/screenshot.h"
#include "world/car_mesh.h"
#include "world/material.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"
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
    MeshBuilder mb;
    Palette palette;
    const bool have_palette =
        !asset_dir_.empty() && load_vga_palette(path_join(asset_dir_, "SDMAIN.PVS"), &palette);
    if (have_palette) {
      world::build_track_static_geometry(track_, lib, mb, palette, &stats);
    } else {
      world::build_track_static_geometry(track_, lib, mb, &stats);
    }
    S3D_INFO("track shapes: ground=%d terrain=%d elements=%d overlays=%d missing=%d",
             stats.ground_cells, stats.terrain_shapes, stats.elements, stats.overlays,
             stats.missing_shapes);

    // Animated objects, one mesh per paint phase (see track_shapes.h).
    world::TrackShapeStats anim_stats;
    for (int phase = 0; phase < world::kTrackAnimationPhases; ++phase) {
      MeshBuilder amb;
      MeshBuilder alb;
      if (have_palette) {
        world::build_track_animated_geometry(track_, lib, amb, palette, phase, &anim_stats, &alb);
      } else {
        world::build_track_animated_geometry(track_, lib, amb, phase, &anim_stats, &alb);
      }
      animated_meshes_[phase] = amb.to_mesh();
      animated_lines_[phase] = alb.to_mesh();
    }
    S3D_INFO("track shapes: %d animated objects", anim_stats.animated);

    // Debug: place the player car at the start/finish cell (item 1) so its
    // size can be compared against the road. The racing car is `car1` inside
    // `ST<ID>.P3S`; see car_mesh.h for the scale notes.
    ShapeLibrary car_lib;
    if (!asset_dir_.empty() && world::load_car_archive(asset_dir_, "COUN", &car_lib)) {
      const glm::vec3 car_pos(world::cell_center_x(7), 0.0f, world::cell_center_z(5));
      // Shade the body with the original material table (rear lights left dark).
      const glm::vec4 backlight = have_palette ? world::material_color(0x2E, palette)
                                               : glm::vec4(0.67f, 0.0f, 0.0f, 1.0f);
      const Palette* pal = have_palette ? &palette : nullptr;
      world::append_car_model(
          mb, car_lib, world::shape_model(car_pos, 0.0f),
          [pal, backlight](uint8_t paint) {
            if (pal != nullptr) return world::car_primitive_color(paint, *pal, backlight);
            return paint == world::kCarBacklightPaint ? backlight
                                                      : world::track_paint_color(paint);
          });
      S3D_INFO("player car: %s at cell (7,5)", car_lib.find("car1") ? "car1" : "car2");
    }
    mesh_ = mb.to_mesh();
  } else {
    S3D_WARN("no GAME1/GAME2 shapes found; falling back to placeholder geometry");
    mesh_ = build_track_mesh(track_);
  }

  const float cx = world::cell_center_x(world::kTrackSize / 2);
  const float cz = world::cell_center_z(world::kTrackSize / 2);
  if (top_down_) {
    camera_.position = glm::vec3(cx, 18000.0f, cz + 3000.0f);
    aim_at(camera_, glm::vec3(cx, 0.0f, cz));
  } else {
    // Debug close-up of the start/finish cell so the car can be compared with
    // the road surface.
    const glm::vec3 car(world::cell_center_x(7), 0.0f, world::cell_center_z(5));
    camera_.position = car + glm::vec3(-1400.0f, 700.0f, -1800.0f);
    aim_at(camera_, car + glm::vec3(0.0f, 60.0f, 0.0f));
  }
  S3D_INFO("debug scene: %s", name_.c_str());
  return true;
}

void DebugTrackScene::update(double dt, const Controls& controls) {
  // Escape backs out of a standalone debug view (it is no longer a hard quit in
  // the input layer; the front-end reuses it as "back").
  if (controls.menu_cancel) {
    finished_ = true;
    return;
  }
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

void DebugTrackScene::fit_top_down_camera(float usable_width, float view_width,
                                          float height) {
  // Frame the whole track. The projection always spans the full framebuffer, so
  // the map shrinks only as the camera rises; the camera must be high enough to
  // fit the map both vertically (the projection's vertical FOV) and inside the
  // `usable_width` pixels the caller keeps clear of a side panel. A slight tilt
  // avoids a degenerate up vector, and the margin leaves a border.
  const float cx = world::cell_center_x(world::kTrackSize / 2);
  const float cz = world::cell_center_z(world::kTrackSize / 2);
  const float half_extent = 0.5f * static_cast<float>(world::kTrackSize) * world::kSquareSize;
  const float margin = 1.15f;
  const float aspect = (height > 0.0f) ? view_width / height : 16.0f / 9.0f;
  const float tan_h = std::tan(glm::radians(camera_.fov_deg * 0.5f));
  const float tan_v = tan_h / std::max(aspect, 0.05f);
  const float need_v = half_extent * margin / tan_v;
  const float need_h =
      half_extent * margin * view_width / std::max(usable_width, 1.0f) / tan_h;
  const float height_needed = std::max(need_h, need_v);
  camera_.position = glm::vec3(cx, height_needed, cz + height_needed * (3000.0f / 18000.0f));
  aim_at(camera_, glm::vec3(cx, 0.0f, cz));
  top_down_target_ = glm::vec3(cx, 0.0f, cz);
}

void DebugTrackScene::render(Renderer& renderer, int width, int height) {
  Camera cam = camera_;
  if (top_down_) {
    // Fit (and centre) the map inside the usable part of the framebuffer, which
    // the front-end narrows to keep the view clear of its side panel.
    const float view_w = static_cast<float>(width);
    const float usable = (overview_w_ > 0.0f) ? std::min(overview_w_, view_w) : view_w;
    if (usable != fit_w_ || view_w != fit_vw_ || static_cast<float>(height) != fit_h_) {
      fit_top_down_camera(usable, view_w, static_cast<float>(height));
      fit_w_ = usable;
      fit_vw_ = view_w;
      fit_h_ = static_cast<float>(height);
    }
    if (usable > 0.0f && usable < view_w) {
      // Slide the camera so the track's centre lands in the middle of the usable
      // rectangle. `right()` is screen-right, so moving the camera that way
      // shifts the subject left on screen (see Camera::view).
      const float ndc = 2.0f * (usable * 0.5f) / view_w - 1.0f;
      const float dist = glm::length(camera_.position - top_down_target_);
      cam.position +=
          cam.right() * (-ndc * dist * std::tan(glm::radians(cam.fov_deg * 0.5f)));
    }
  }

  renderer.draw_world(mesh_, cam);

  // Cycle the animated objects; /3 keeps the pace close to the 20 Hz game rate.
  const int phase = world::track_animation_phase(frame_index_ / 3);
  renderer.draw_world(animated_meshes_[phase], cam);
  renderer.draw_world_lines(animated_lines_[phase], cam);

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
      if (top_down_) {
        fit_w_ = fit_h_ = fit_vw_ = 0.0f;  // force a viewport-fitted camera
      } else {
        camera_.position = glm::vec3(cx - 9000.0f, 6500.0f, cz - 12000.0f);
        aim_at(camera_, glm::vec3(cx, 0.0f, cz));
      }
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
