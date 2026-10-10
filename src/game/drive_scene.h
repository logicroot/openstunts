#pragma once
// In-race drive scene: engine/steering/grip/motion physics at the original
// 20 Hz rate, rendered at the application's 60 Hz frame rate with pose
// interpolation, plus the follow camera and keyboard input.

#include <string>

#include <glm/glm.hpp>

#include "assets/car.h"
#include "assets/trk.h"
#include "game/scene.h"
#include "physics/car_state.h"
#include "physics/track_collision.h"
#include "render/camera.h"
#include "render/mesh.h"

namespace s3d {

class DriveScene : public Scene {
 public:
  DriveScene(Track track, std::string asset_dir);

  bool on_enter() override;
  void update(double dt, const Controls& controls) override;
  void render(Renderer& renderer, int width, int height) override;

  // Render `frames` frames, then write a PNG and quit (CI/verification).
  void request_screenshot(std::string path, int frames);

  std::string next_scene() const override { return finished_ ? "quit" : std::string(); }

 private:
  void reset_car();
  void physics_tick(const Controls& controls);
  void update_camera();

  // Interpolated visual pose for the current render frame.
  float render_alpha() const;
  glm::vec3 car_world_position(float alpha) const;
  float car_yaw(float alpha) const;
  float car_pitch(float alpha) const;
  float car_roll(float alpha) const;
  glm::mat4 car_model_matrix(float alpha) const;

  Track track_;
  std::string asset_dir_;

  CarSpec spec_;
  physics::CarState car_;
  physics::CarState display_prev_;  // pose at the last physics tick boundary
  double accumulator_ = 0.0;        // leftover time since the last physics tick
  physics::TrackCollision collision_;  // element planes/walls from GAME.PRE

  Mesh track_mesh_;
  Mesh car_mesh_;
  Camera camera_;

  std::string screenshot_path_;
  int screenshot_frame_ = 0;
  int frame_index_ = 0;
  bool finished_ = false;
};

}  // namespace s3d