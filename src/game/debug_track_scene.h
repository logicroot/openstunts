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
  bool top_down_ = false;
  bool finished_ = false;
};

}  // namespace s3d
