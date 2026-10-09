#pragma once
// Debug scene: free-fly inspection of a track and its placeholder geometry.

#include <string>

#include "assets/trk.h"
#include "game/scene.h"
#include "render/camera.h"
#include "render/mesh.h"

namespace s3d {

class DebugTrackScene : public Scene {
 public:
  DebugTrackScene(Track track, std::string name);

  bool on_enter() override;
  void update(double dt, const Controls& controls) override;
  void render(Renderer& renderer, int width, int height) override;
  void handle_event(const SDL_Event& e) override;

  // Render `frames` frames, then write a PNG and quit. Used for CI/verification.
  void request_screenshot(std::string path, int frames);

  std::string next_scene() const override { return finished_ ? "quit" : std::string(); }

 private:
  Track track_;
  std::string name_;
  Camera camera_;
  Mesh mesh_;
  std::string screenshot_path_;
  int screenshot_frame_ = 0;
  int frame_index_ = 0;
  int hotkey_shots_ = 0;
  bool mouse_look_ = false;
  bool pending_hotkey_ = false;
  bool finished_ = false;
};

}  // namespace s3d
