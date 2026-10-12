#pragma once
// In-race drive scene: engine/steering/grip/motion physics at the original
// 20 Hz rate, rendered at the application's 60 Hz frame rate with pose
// interpolation, plus the follow camera and keyboard input.

#include <string>

#include <glm/glm.hpp>

#include "assets/car.h"
#include "assets/crash_overlay.h"
#include "assets/font.h"
#include "assets/palette.h"
#include "assets/sprite2d.h"
#include "assets/trk.h"
#include "game/race_state.h"
#include "game/scene.h"
#include "physics/car_state.h"
#include "physics/track_collision.h"
#include "render/camera.h"
#include "render/hud.h"
#include "render/mesh.h"
#include "world/track_shapes.h"

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

  // Builds the in-game HUD quads (crash explosion, crack/sinking overlay, race
  // clock, direction arrow/warning, penalty banner) in pixel coordinates.
  // `car_rect` is the clipped car screen rectangle (left, top, right, bottom)
  // or nullptr when the car is not visible; it drives the crash explosion.
  void build_hud(std::vector<Vertex>& verts, int width, int height,
                 const glm::vec4* car_rect) const;

  // Projects the car's local bounding box into a viewport-clipped screen
  // rectangle (left, top, right, bottom) in top-left pixel coordinates. Returns
  // false when the box is behind the camera or fully off-screen.
  bool car_screen_rect(const Camera& camera, float alpha, int width, int height,
                       glm::vec4* rect) const;

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
  physics::CarState display_prev_;  // car pose at the previous physics tick
  double accumulator_ = 0.0;        // leftover time since the last physics tick
  physics::TrackCollision collision_;  // element planes/walls from GAME.PRE
  Palette palette_;                    // VGA colours for track materials

  Mesh track_mesh_;  // static terrain + non-animated elements
  // Animated objects (windmill sails, flashing sign, ...) prebuilt for each of
  // the 4 paint phases; the game frame selects one to draw.
  Mesh animated_meshes_[world::kTrackAnimationPhases];
  Mesh animated_lines_[world::kTrackAnimationPhases];
  int game_frame_ = 0;  // advances with the 20 Hz physics ticks

  Mesh car_mesh_;
  // Same geometry as car_mesh_ but with the rear lights lit (material 0x2F
  // instead of 0x2E); drawn while the car is braking.
  Mesh car_mesh_braking_;
  // Player car local bounding box (car_mesh_ vertices, identity model), used to
  // project the car to screen for the crash explosion.
  glm::vec3 car_bounds_min_{0.0f};
  glm::vec3 car_bounds_max_{0.0f};
  bool car_bounds_valid_ = false;
  Camera camera_;
  Camera camera_prev_;  // camera pose at the previous physics tick

  // Race loop: start/finish line, 20 Hz clock and the finish state.
  game::StartFinish start_finish_;
  game::RaceState race_;
  BitmapFont hud_font_;   // FONTLED.FNT, the LED race-clock font
  BitmapFont text_font_;  // FONTDEF.FNT, the in-game banner/warning font
  Sprite2D arrow_left_;    // SDGAME2.PVS "left"
  Sprite2D arrow_right_;   // SDGAME2.PVS "righ"
  Sprite2D explosion_[3];  // SDGAME2.PVS "ex01"/"ex02"/"ex03" crash animation
  CrashOverlay crash_;    // GAME.PRE "crak"/"cinf": crashed-windscreen overlay
  int crash_end_frame_ = -1;  // game_pEndFrame: tick the crash started (any flag)
  HudRenderer hud_;
  glm::vec4 hud_fg_{1.0f, 1.0f, 1.0f, 1.0f};      // palette index 15
  glm::vec4 hud_shadow_{0.0f, 0.0f, 0.0f, 1.0f};  // palette index 0

  std::string screenshot_path_;
  int screenshot_frame_ = 0;
  int frame_index_ = 0;
  bool finished_ = false;
};

}  // namespace s3d