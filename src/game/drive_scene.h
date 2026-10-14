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
#include "assets/opponent.h"
#include "assets/palette.h"
#include "assets/sprite2d.h"
#include "assets/trk.h"
#include "core/kevin_random.h"
#include "game/race_state.h"
#include "game/scene.h"
#include "physics/car_collision.h"
#include "physics/car_debris.h"
#include "physics/car_state.h"
#include "physics/opponent_ai.h"
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
  // `car_id` is the player car (the original's gameconfig.game_playercarid),
  // e.g. "COUN" or "VETT"; it selects CAR<ID>.RES, ST<ID>.P3S and STDA<ID>.PVS.
  // `opponent_car_id` (gameconfig.game_opponentcarid) selects the opponent car;
  // when empty the race runs without an opponent. `opponent_type` is the
  // opponent driving style (1..6) and picks the OPPn.PRE sped table.
  DriveScene(Track track, std::string asset_dir, std::string car_id = "COUN",
             std::string opponent_car_id = "", int opponent_type = 1);

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

  // (Re)builds the opponent car meshes for the current `opponent_wheel_pose_`
  // (the same sub_204AE wheel pass; only the shape archive differs).
  void build_opponent_meshes();

  // Re-seeds the opponent car state at the start/finish line (restunts.c
  // init_game_state): the other flank of the grid, facing -track_angle, with
  // its first course checkpoint written. No-op without an opponent.
  void reset_opponent_car();

  // The AI's per-race inputs: the route graph, the OPPn.PRE table and the
  // course path (cached in `opponent_path_`).
  physics::OpponentRaceSetup opponent_setup() const;
  // Recomputes the cached course path (trackdata3) after a race (re)set-up.
  void build_opponent_path();

  // Builds the in-game HUD quads (crash explosion, crack/sinking overlay, race
  // clock, direction arrow/warning, opponent-near indicator, penalty banner) in
  // pixel coordinates. `car_rect` is the clipped car screen rectangle (left,
  // top, right, bottom) or nullptr when the car is not visible; it drives the
  // crash explosion.
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

  // Draws the speedometer and tachometer needles over the instrument face:
  // single-colour lines from SIMD spdcenter/revcenter to the point selected by
  // the car's speed and rpm (seg005.asm loc_23456/loc_23485, meter_needle_color
  // = palette 15). No-op for digital/absent speedometers.
  void draw_instrument_needles(const world::DashboardRects& r, int width,
                               int height);

  // Stamps the digital speedometer glyphs over the instrument face (seg005.asm
  // loc_2338C): car_speed >> 8 split into up to three "dig" glyphs at the SIMD
  // spdpoints[0..5] positions, leading zeros suppressed. No-op when the car has
  // an analog/absent speedometer or the glyphs were not loaded.
  void draw_digital_speedo(const world::DashboardRects& r, int width, int height);

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
  // The opponent car's interpolated world position and body-to-world matrix
  // (same lerp and mat_rot_zxy(-roll, -pitch, -yaw) pose as the player car).
  glm::vec3 opponent_world_position(float alpha) const;
  glm::mat4 opponent_model_matrix(float alpha) const;

  // Builds the four crash-debris shard meshes ("exp0".."exp3") for the scenery
  // archive (GAME1/GAME2), the player car (ST<ID>.P3S) and, when an opponent
  // exists, the opponent car. Debris meshes are only useful once the physics
  // can spawn pieces, so the call sits after the car setup in on_enter.
  void build_debris_meshes(const ShapeLibrary& track_lib);
  // Interpolated world matrix of one debris piece: the original places it at
  // `(game_longs + owner_car_posWorld) >> 6` -- the owner car's position, the
  // player's for field_443 = 0 and the opponent's for = 1 -- and tumbles it
  // with mat_rot_zxy(-field_2FE, -field_32E, -field_35E).
  glm::mat4 debris_model_matrix(const physics::DebrisPiece& prev,
                                const physics::DebrisPiece& curr, float alpha) const;
  // The owning car's interpolated world position for one debris piece (frame.c
  // places each piece at game_longs + owner_car_posWorld).
  glm::vec3 debris_owner_position(uint8_t owner, float alpha) const;

  Track track_;
  std::string asset_dir_;
  std::string car_id_;  // player car id, upper-cased (e.g. "COUN", "VETT")

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

  // ---- Opponent car (gameconfig.game_opponenttype != 0) --------------------
  // The second racing car: its own SIMD profile (CAR<ID>.RES), shape archive
  // (ST<ID>.P3S) and OPPn.PRE course table. `opponent_` carries the physics
  // state the AI module (opponent_ai.h) advances; `opponent_prev_` is its pose
  // at the previous physics tick for render interpolation. The bodywork is
  // drawn in every camera mode (frame.c draws it whenever game_opponenttype
  // != 0), unlike the player's own body which hides in the cockpit.
  std::string opponent_car_id_;  // upper-cased car id (e.g. "VETT")
  int opponent_type_ = 1;        // OPPn.PRE index (1..6)
  bool have_opponent_ = false;
  OpponentData opponent_data_;
  CarSpec opponent_spec_;
  physics::CarState opponent_;
  physics::CarState opponent_prev_;
  // Both cars' collide_points boxes (physics::collision_points_from_spec), used
  // by the car-vs-car test in physics_tick.
  physics::CollisionPoints player_collision_points_;
  physics::CollisionPoints opponent_collision_points_;
  physics::OpponentAiState opponent_ai_;
  // state.field_45E: the last run_opponent_ai return (0 none, 1 first-edge
  // side, 2 second-edge side), driving the HUD opponent-near indicator.
  int opponent_field_45e_ = 0;
  // The AI's course path (trackdata3): race_.route.opponent_path() over the
  // opponent's sped table. Rebuilt when the race is (re)set up.
  std::vector<uint16_t> opponent_path_;
  std::shared_ptr<ShapeLibrary> opponent_lib_;
  Mesh opponent_mesh_;          // the racing body, rear lights off
  Mesh opponent_mesh_braking_;  // rear lights on (car_is_braking)
  world::CarWheelPose opponent_wheel_pose_;

  // Crash debris (restunts state_op_unk / sub_19BA0): up to 24 shards spawned
  // when the player car enters the crash state, simulated at 20 Hz and drawn as
  // the car's own "exp0".."exp3" pieces. `debris_meshes_[archive][shard]` are
  // prebuilt in local space: archive 0 = GAME1/GAME2 scenery shards, 1 = the
  // player car, 2 = the opponent (not loaded until the opponent exists).
  physics::DebrisField debris_;
  physics::DebrisField debris_prev_;  // debris state at the previous physics tick
  KevinRandom kevin_rng_;             // get_kevinrandom, seeded "kevin"
  Mesh debris_meshes_[3][4];
  // exp2 shards are a bare 2-vertex line in ST<ID>.P3S; they are drawn with
  // draw_world_lines rather than as a triangle.
  Mesh debris_line_meshes_[3][4];

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
  // The instrument face ("ins2"), gear-selector plate ("gbox"), gear lever
  // ("gnob") and steering dot ("dot"); the needles/knob/dot are drawn over the
  // panel each frame (seg005.asm setup_car_shapes case 2).
  Texture dashboard_instrument_texture_;  // "ins2"
  Texture dashboard_gear_box_texture_;    // "gbox"
  Texture dashboard_knob_texture_;        // "gnob"
  Texture dashboard_knob_mask_texture_;   // "gnab" AND stencil for the knob
  Texture dashboard_dot_texture_;         // "dot"
  Texture dashboard_dot_mask_texture_;    // "dota" AND stencil for the dot
  // The turned-wheel face patches: [0] = "ins1" (wheel held left), [1] = "ins3"
  // (held right), and their "inm1"/"inm3" AND stencils. The patches are OR-blits
  // in which palette index 0 is a no-op, so their textures keep index 0
  // transparent; the stencils are stamped opaquely (index 0 = black).
  Texture dashboard_patch_textures_[2];
  Texture dashboard_patch_mask_textures_[2];
  Texture dashboard_digit_textures_[10];  // "dig0".."dig9" (STDB)
  // The per-frame "dast" strip and its "dasm" stencil (absolute coordinates).
  Texture dashboard_dast_texture_;
  Texture dashboard_dast_mask_texture_;
  bool dashboard_on_ = true;

  // Camera mode: 0 = cockpit, 1-3 = chase/varied.
  int camera_mode_ = 0;  // default to cockpit (cameramode 0)

  std::string screenshot_path_;
  int screenshot_frame_ = 0;
  int frame_index_ = 0;
  bool finished_ = false;
};

}  // namespace s3d