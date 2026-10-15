#pragma once
// Debug scene: free-fly inspection of a track and its placeholder geometry.

#include <string>

#include "assets/trk.h"
#include "game/scene.h"
#include "render/camera.h"
#include "render/mesh.h"
#include "world/track_shapes.h"

namespace s3d {

class DebugTrackScene : public Scene {
 public:
  DebugTrackScene(Track track, std::string name, std::string asset_dir = {});

  bool on_enter() override;
  void update(double dt, const Controls& controls) override;
  void render(Renderer& renderer, int width, int height) override;
  void handle_event(const SDL_Event& e) override;

  // Render `frames` frames, then write a PNG and quit. Used for CI/verification.
  void request_screenshot(std::string path, int frames);

  // Start with a top-down overview camera (useful for inspecting track layout).
  void set_top_down(bool value) { top_down_ = value; }

  // Restrict the top-down overview to the leftmost `width_px` pixels of the
  // framebuffer (so a side panel does not cover it) and centre the track there.
  // 0 (the default) keeps the whole framebuffer.
  void set_overview_width(float width_px) { overview_w_ = width_px; }

  std::string next_scene() const override { return finished_ ? "quit" : std::string(); }

 private:
  Track track_;
  std::string name_;
  std::string asset_dir_;
  Camera camera_;
  Mesh mesh_;
  Mesh animated_meshes_[world::kTrackAnimationPhases];
  Mesh animated_lines_[world::kTrackAnimationPhases];
  std::string screenshot_path_;
  int screenshot_frame_ = 0;
  int frame_index_ = 0;
  int hotkey_shots_ = 0;
  bool mouse_look_ = false;
  bool pending_hotkey_ = false;
  // Track the last viewport size for top-down camera fitting.
  float fit_w_ = 0.0f;
  float fit_vw_ = 0.0f;
  float fit_h_ = 0.0f;
  float overview_w_ = 0.0f;
  glm::vec3 top_down_target_{0.0f};
  bool top_down_ = false;
  bool finished_ = false;

  // Place the top-down camera so the whole track fits the `usable_width` pixels
  // of a `view_width`-wide framebuffer without clipping. The standalone debug
  // view allows WASD movement, so the fit runs only when the viewport changes.
  void fit_top_down_camera(float usable_width, float view_width, float height);
};

}  // namespace s3d
