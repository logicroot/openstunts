#pragma once
// Front-end: the Minecraft-styled start menu and its sub-screens.
//
// Screens: Main, Tracks, Car, Opponent, Options. The main screen runs an
// attract-mode demo (a DriveScene driven by the AI autopilot) behind the menu.
// Navigation is keyboard/gamepad only: Up/Down move (wrapping over disabled
// rows), Left/Right cycle a value row, Enter activates and Escape backs out.
//
// Reference note: the original Stunts front-end is not part of restunts, so this
// scene is new work; it reuses the port's assets (FONTDEF.FNT, the tracks, the
// car archives, SDOSEL.PVS opponent faces) and, for the track preview, the
// existing DebugTrackScene top-down view.

#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "assets/font.h"
#include "assets/palette.h"
#include "assets/shape3d.h"
#include "assets/sprite2d.h"
#include "assets/trk.h"
#include "core/config.h"
#include "game/scene.h"
#include "render/camera.h"
#include "render/mesh.h"
#include "render/sprite_renderer.h"
#include "render/texture.h"
#include "ui/gui.h"
#include "ui/menu.h"
#include "world/shape_mesh.h"

namespace s3d {

class DebugTrackScene;
class DriveScene;
class Renderer;

class FrontEndScene : public Scene {
 public:
  // `config` is mutated with the player's selections (and saved to
  // `config_path` on exit when it is non-empty).
  FrontEndScene(Config* config, std::string config_path, std::string asset_dir);
  ~FrontEndScene() override;

  bool on_enter() override;
  void on_exit() override;
  void update(double dt, const Controls& controls) override;
  void render(Renderer& renderer, int width, int height) override;

  std::string next_scene() const override { return next_; }

  // Render `frames` frames, then write a PNG and quit (CI/verification).
  void request_screenshot(std::string path, int frames);
  // Start on a given screen (0 = main, 1 = tracks, 2 = car, 3 = opponent,
  // 4 = options); used by the screenshot tooling.
  void set_start_screen(int screen) { start_screen_ = screen; }

 private:
  enum class Screen { kMain, kTracks, kCar, kOpponent, kOptions };
  // Whether the Car screen is editing the player's or the opponent's car.
  enum class CarTarget { kPlayer, kOpponent };

  void set_screen(Screen s);
  void build_menu();
  void handle_input(const Controls& controls);
  void on_selection_changed();
  void activate();
  void go_back();
  // Advance the player's or opponent's car selection by `delta` (wrapping) and
  // move the menu cursor onto it.
  void cycle_car(int delta);
  // Apply a Left/Right change on the current row (called after menu_.adjust).
  void apply_value(int row);

  // Backgrounds / previews (lazily built for the active screen).
  void ensure_demo();
  void ensure_track_preview();
  void ensure_car_preview();
  void ensure_opponent_face();

  // Rendering helpers.
  void draw_block_background(int width, int height);
  void draw_car_preview(Renderer& renderer, int width, int height);
  void draw_opponent_face(int width, int height);
  void render_overlay(int width, int height);
  // Panel geometry for the current screen (view units): the Car/Tracks/
  // Opponent screens push it to the right so the preview stays visible.
  struct PanelLayout {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    float button_h = 0.0f;
    float button_gap = 0.0f;
    float title_h = 0.0f;
    float hint_h = 0.0f;
  };
  PanelLayout panel_layout() const;
  bool panel_on_right() const;
  float preview_center_vx(const PanelLayout& layout) const;
  std::string screen_title() const;
  std::string screen_hint() const;

  void scan_tracks();
  void scan_cars();
  void load_palette();

  Config* config_ = nullptr;
  std::string config_path_;
  std::string asset_dir_;

  Screen screen_ = Screen::kMain;
  CarTarget car_target_ = CarTarget::kPlayer;
  ui::MenuModel menu_;
  std::string next_;

  ui::Gui gui_;
  Texture bg_texture_;         // tiled block background (non-main screens)
  std::vector<Vertex> bg_quad_;
  bool gui_ready_ = false;

  Palette palette_;
  bool have_palette_ = false;

  std::vector<std::string> tracks_;
  std::vector<std::string> cars_;

  // Attract-mode demo (main screen background).
  std::unique_ptr<DriveScene> demo_;
  bool demo_tried_ = false;

  // Track preview: a reusable top-down DebugTrackScene, rebuilt per selection.
  std::unique_ptr<DebugTrackScene> track_preview_;
  int track_preview_index_ = -1;

  // Car preview: a static turntable mesh re-built per selection.
  Mesh car_preview_mesh_;
  Camera car_preview_cam_;
  glm::vec3 car_preview_focus_{0.0f, 160.0f, 0.0f};
  int car_preview_index_ = -1;
  bool car_preview_valid_ = false;

  // Opponent face.
  Sprite2D face_sprite_;
  Texture face_texture_;
  std::vector<Vertex> face_quad_;
  int face_index_ = -1;

  float preview_angle_ = 0.0f;  // car turntable spin (degrees)

  std::string screenshot_path_;
  int screenshot_frame_ = 0;
  int frame_index_ = 0;
  int start_screen_ = 0;
};

}  // namespace s3d
