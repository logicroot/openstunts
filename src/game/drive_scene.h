#pragma once
// In-race drive scene: engine/steering/grip/motion physics at the original
// 20 Hz rate, rendered at the application's 60 Hz frame rate with pose
// interpolation, plus the follow camera and keyboard input.

#include <memory>
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
#include "render/sprite_renderer.h"
#include "render/texture.h"
#include "world/car_mesh.h"
#include "world/dashboard.h"
#include "world/skybox.h"
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

  // (Re)builds the player car meshes for the current `wheel_pose_`: the bodywork
  // plus the four wheels steered/travelled exactly as the original `sub_204AE`.
  // Also refreshes car_bounds_* the first time.
  void build_car_meshes();

  // Builds the in-game HUD quads (crash explosion, crack/sinking overlay, race
  // clock, direction arrow/warning, penalty banner) in pixel coordinates.
  // `car_rect` is the clipped car screen rectangle (left, top, right, bottom)
  // or nullptr when the car is not visible; it drives the crash explosion.
  void build_hud(std::vector<Vertex>& verts, int width, int height,
                 const glm::vec4* car_rect) const;

  // Draws the sky/ground fills and the horizon mountain panorama behind the
  // world (seg003.asm skybox_op / skybox_op_helper2). No-op when the skybox
  // asset is absent.
  void draw_sky(const Camera& camera, float alpha, int width, int height);

  // Draws the cockpit dashboard panel (STDA<ID>.PVS "roof" + "dash") opaquely
  // over the screen bands outside the windshield. No-op when the dashboard is
  // hidden or the asset was not loaded.
  void draw_dashboard(int width, int height);

  // Screen row (pixels, top-left origin) of the horizon for `camera`: the
  // projection of the world-horizontal direction, i.e. where the view ray is
  // level. The original derives the same line from two far ground points.
  float horizon_row(const Camera& camera, int width, int height) const;

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
  bool have_palette_ = false;

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
  // Player car shape archive (STCOUN.P3S) and the wheel pose the meshes were
  // last built with (frame.c sub_204AE: steering + per-wheel suspension).
  std::shared_ptr<ShapeLibrary> car_lib_;
  world::CarWheelPose wheel_pose_;
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

  // Horizon scenery (seg003.asm load_skybox): the track's theme archive plus its
  // four mountain layers uploaded as RGBA textures, and the flat sky/ground
  // colours read from material_color_list.
  world::Skybox skybox_;
  Texture skybox_textures_[world::kSkyboxLayerCount];
  SpriteRenderer sprites_;
  glm::vec4 sky_color_{0.55f, 0.72f, 0.92f, 1.0f};
  glm::vec4 ground_color_{0.14f, 0.41f, 0.13f, 1.0f};

  // Cockpit dashboard (seg005.asm setup_car_shapes / loop_game): the car's
  // STDA<ID>.PVS "dash"/"roof" panels, drawn opaquely over the lower/upper
  // screen bands. `dashb_toggle` is 1 (shown) for a normal race; the panel hides
  // the world outside the windshield and the HUD clock moves below the roof.
  world::Dashboard dashboard_;
  Texture dashboard_textures_[2];  // [0] = dash, [1] = roof
  Texture dashboard_wheel_textures_[3];  // "whl1"/"whl2"/"whl3"
  bool dashboard_on_ = true;

  std::string screenshot_path_;
  int screenshot_frame_ = 0;
  int frame_index_ = 0;
  bool finished_ = false;
};

}  // namespace s3d