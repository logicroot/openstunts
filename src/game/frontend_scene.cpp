#include "game/frontend_scene.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

#include "assets/car.h"
#include "core/log.h"
#include "core/paths.h"
#include "game/debug_track_scene.h"
#include "game/drive_scene.h"
#include "platform/input.h"
#include "render/renderer.h"
#include "render/screenshot.h"
#include "world/car_mesh.h"
#include "world/material.h"
#include "world/mesh_builder.h"

namespace s3d {

namespace {

// The attract demo loops on this track with this car (Config::last_track when
// the player has never changed it is DEFAULT, which has no start/finish gate).
constexpr const char* kDemoTrack = "HELENS.TRK";
constexpr const char* kDemoCar = "PMIN";

// How many drivers the Opponent screen can cycle through (OPP1..OPP6); index 0
// is always Race against the Clock.
constexpr int kOpponentSlots = 6;

// A 4-character car id from a CAR*.RES name.
std::string car_id_from_file(const std::string& file) {
  if (file.size() <= 7) return {};
  return file.substr(3, file.size() - 7);  // "CAR" + id + ".RES"
}

bool file_readable(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return f.good();
}

std::string upper(std::string s) {
  for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return s;
}

// A textured quad (top-left origin, pixel space) for the sprite renderer.
std::vector<Vertex> make_tex_quad(float x, float y, float w, float h) {
  std::vector<Vertex> out;
  Vertex v;
  v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  const glm::vec3 pos[6] = {{x, y, 0.0f},     {x + w, y, 0.0f},     {x + w, y + h, 0.0f},
                            {x, y, 0.0f},     {x + w, y + h, 0.0f}, {x, y + h, 0.0f}};
  const glm::vec2 uv[6] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
                           {0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  for (int i = 0; i < 6; ++i) {
    v.position = pos[i];
    v.uv = uv[i];
    out.push_back(v);
  }
  return out;
}

// Points a camera at `target` (the same computation DebugTrackScene uses).
void aim_at(Camera& cam, const glm::vec3& target) {
  const glm::vec3 d = glm::normalize(target - cam.position);
  cam.yaw_deg = glm::degrees(std::atan2(d.z, d.x));
  cam.pitch_deg = glm::degrees(std::asin(d.y));
}

}  // namespace

FrontEndScene::FrontEndScene(Config* config, std::string config_path,
                             std::string asset_dir)
    : config_(config),
      config_path_(std::move(config_path)),
      asset_dir_(std::move(asset_dir)) {
  // These only read the filesystem, so they run at construction: the menus and
  // the unit tests see the real track/car lists without needing a GL context.
  scan_tracks();
  scan_cars();
}

FrontEndScene::~FrontEndScene() = default;

bool FrontEndScene::on_enter() {
  gui_.init(asset_dir_);
  gui_ready_ = true;
  load_palette();

  // The block tile drawn behind the sub-screens.
  {
    std::vector<uint8_t> pixels;
    ui::generate_block_texture(ui::BlockTexture::kDirt, ui::kBlockTextureSize,
                               ui::kBlockSeed, &pixels);
    TextureDesc desc;
    desc.filter = TextureFilter::Nearest;
    desc.wrap = TextureWrap::Repeat;
    bg_texture_.upload_rgba(ui::kBlockTextureSize, ui::kBlockTextureSize,
                            pixels.data(), desc);
  }

  set_screen(Screen::kMain);
  if (start_screen_ > 0 && start_screen_ <= static_cast<int>(Screen::kOptions)) {
    set_screen(static_cast<Screen>(start_screen_));
  }
  S3D_INFO("front-end ready: %zu tracks, %zu cars", tracks_.size(), cars_.size());
  return true;
}

void FrontEndScene::on_exit() {
  // Persist the player's selections (best-effort).
  if (config_ != nullptr) config_->save(config_path_);
}

void FrontEndScene::load_palette() {
  if (!asset_dir_.empty() &&
      load_vga_palette(path_join(asset_dir_, "SDMAIN.PVS"), &palette_)) {
    have_palette_ = true;
  }
}

void FrontEndScene::scan_tracks() {
  // Retail tracks first, then any other *.TRK present (so "More Tracks" picks
  // up files the player added to the data directory).
  const char* const retail[] = {"BERNIES.TRK", "CHERRIS.TRK", "DEFAULT.TRK",
                                "HELENS.TRK",  "JOES.TRK",    "SKIDS.TRK"};
  tracks_.clear();
  for (const char* t : retail) {
    if (file_readable(path_join(asset_dir_, t))) tracks_.push_back(t);
  }
  // Everything past the retail block is a file the player dropped in the data
  // directory; remember where that block ends so only the extras are sorted.
  const size_t retail_count = tracks_.size();
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(asset_dir_, ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    const std::string name = upper(entry.path().filename().string());
    if (name.size() < 5 || name.compare(name.size() - 4, 4, ".TRK") != 0) continue;
    if (std::find(tracks_.begin(), tracks_.end(), name) == tracks_.end()) {
      tracks_.push_back(name);
    }
  }
  // Retail tracks are already in place; the extras are sorted for stability.
  std::sort(tracks_.begin() + static_cast<ptrdiff_t>(retail_count), tracks_.end());
}

void FrontEndScene::scan_cars() {
  cars_.clear();
  int count = 0;
  const char* const* files = retail_car_files(&count);
  for (int i = 0; i < count; ++i) {
    const std::string id = car_id_from_file(files[i]);
    if (!id.empty() && file_readable(path_join(asset_dir_, files[i]))) {
      cars_.push_back(id);
    }
  }
  if (cars_.empty()) cars_.push_back(config_ != nullptr ? config_->player_car : "COUN");
}

std::string FrontEndScene::screen_title() const {
  switch (screen_) {
    case Screen::kMain: return "OpenStunts";
    case Screen::kTracks: return "Select Track";
    case Screen::kCar:
      return car_target_ == CarTarget::kPlayer ? "Select Car" : "Opponent Car";
    case Screen::kOpponent: return "Select Opponent";
    case Screen::kOptions: return "Options";
  }
  return {};
}

void FrontEndScene::set_screen(Screen s) {
  screen_ = s;
  build_menu();
  // A new screen starts focused on its first row (landing on an enabled row),
  // not on whatever row happened to be selected on the previous screen.
  menu_.set_selected(0);
  on_selection_changed();
}

void FrontEndScene::build_menu() {
  std::vector<ui::MenuItem> items;
  switch (screen_) {
    case Screen::kMain: {
      items.push_back({"Let's Drive"});
      items.push_back({"Tracks"});
      items.push_back({"Car"});
      items.push_back({"Opponent"});
      items.push_back({"Options"});
      items.push_back({"Exit"});
      break;
    }
    case Screen::kTracks: {
      ui::MenuItem more;
      more.label = "More Tracks";
      for (const std::string& t : tracks_) {
        more.values.push_back(track_label(t));
      }
      if (config_ != nullptr && !config_->last_track.empty()) {
        const std::string want = track_label(config_->last_track);
        for (size_t i = 0; i < more.values.size(); ++i) {
          if (more.values[i] == want) {
            more.value_index = static_cast<int>(i);
            break;
          }
        }
      }
      items.push_back(std::move(more));
      ui::MenuItem edit;
      edit.label = "New / Edit Tracks";
      edit.enabled = false;  // placeholder
      items.push_back(std::move(edit));
      items.push_back({"Main Menu"});
      break;
    }
    case Screen::kCar: {
      // The original front-end does not list model ids: the car itself is the
      // preview and Next/Last walk through the archives. Keep the same set of
      // options the design calls for.
      items.push_back({"Done"});
      items.push_back({"Next"});
      items.push_back({"Last"});
      // Auto/Manual Transmission is wider than the button at this font size,
      // so it is a value row (as the original shows the setting) with the
      // transmission value on the right.
      ui::MenuItem trans;
      trans.label = "Transmission";
      trans.values = {"Auto", "Manual"};
      trans.enabled = false;  // placeholder
      items.push_back(std::move(trans));
      ui::MenuItem colour;
      colour.label = "Colour";
      colour.enabled = false;  // placeholder
      items.push_back(std::move(colour));
      break;
    }
    case Screen::kOpponent: {
      // Like the Car screen: "Done" comes first so returning is the default.
      // The drivers are not a list: the face shows the selection, Last/Next
      // walk through them, and "Clock" sets the race against the Clock.
      items.push_back({"Done"});
      items.push_back({"Last"});
      items.push_back({"Next"});
      items.push_back({"Clock"});
      items.push_back({"Car"});
      break;
    }
    case Screen::kOptions: {
      ui::MenuItem res;
      res.label = "Resolution";
      res.values = {"1280x720", "1600x900", "1920x1080", "2560x1440"};
      if (config_ != nullptr) {
        const std::string cur =
            std::to_string(config_->width) + "x" + std::to_string(config_->height);
        for (size_t i = 0; i < res.values.size(); ++i) {
          if (res.values[i] == cur) res.value_index = static_cast<int>(i);
        }
      }
      items.push_back(std::move(res));

      ui::MenuItem fs;
      fs.label = "Fullscreen";
      fs.values = {"Off", "On"};
      fs.value_index = (config_ != nullptr && config_->fullscreen) ? 1 : 0;
      items.push_back(std::move(fs));

      ui::MenuItem ws;
      ws.label = "Wheel Scale";
      ws.values = {"1.00x", "1.25x", "1.50x", "1.75x"};
      if (config_ != nullptr) {
        const float w = config_->wheel_scale;
        ws.value_index = (w < 1.12f) ? 0 : (w < 1.37f) ? 1 : (w < 1.62f) ? 2 : 3;
      }
      items.push_back(std::move(ws));

      ui::MenuItem pat;
      pat.label = "Patterned Surfaces";
      pat.values = {"Off", "On"};
      pat.value_index = (config_ != nullptr && config_->pattern_fill) ? 1 : 0;
      items.push_back(std::move(pat));

      ui::MenuItem fps;
      fps.label = "Show FPS";
      fps.values = {"Off", "On"};
      fps.value_index = (config_ != nullptr && config_->show_fps) ? 1 : 0;
      items.push_back(std::move(fps));

      ui::MenuItem audio;
      audio.label = "Audio";
      audio.enabled = false;  // placeholder: the port has no audio system yet
      items.push_back(std::move(audio));

      items.push_back({"Main Menu"});
      break;
    }
  }
  menu_.set_items(std::move(items));
}

void FrontEndScene::on_selection_changed() {
  switch (screen_) {
    case Screen::kTracks: ensure_track_preview(); break;
    case Screen::kCar: ensure_car_preview(); break;
    case Screen::kOpponent: ensure_opponent_face(); break;
    default: break;
  }
}

void FrontEndScene::inject_menu_up() { Controls c{}; c.menu_up = true; handle_input(c); }
void FrontEndScene::inject_menu_down() { Controls c{}; c.menu_down = true; handle_input(c); }
void FrontEndScene::inject_menu_left() { Controls c{}; c.menu_left = true; handle_input(c); }
void FrontEndScene::inject_menu_right() { Controls c{}; c.menu_right = true; handle_input(c); }
void FrontEndScene::inject_menu_accept() { Controls c{}; c.menu_accept = true; handle_input(c); }
void FrontEndScene::inject_menu_cancel() { Controls c{}; c.menu_cancel = true; handle_input(c); }

void FrontEndScene::handle_input(const Controls& controls) {
  if (controls.menu_up) {
    menu_.move_up();
    on_selection_changed();
  }
  if (controls.menu_down) {
    menu_.move_down();
    on_selection_changed();
  }
  if (controls.menu_left) {
    menu_.adjust(-1);
    apply_value(menu_.selected());
  }
  if (controls.menu_right) {
    menu_.adjust(1);
    apply_value(menu_.selected());
  }
  if (controls.menu_accept) activate();
  if (controls.menu_cancel) go_back();

  handle_mouse(controls);
}

std::string FrontEndScene::track_label(const std::string& file) {
  std::string label = file;
  const std::string up = upper(label);
  if (up.size() >= 5 && up.compare(up.size() - 4, 4, ".TRK") == 0) {
    label.resize(label.size() - 4);
  }
  return label;
}

void FrontEndScene::select_screen(int screen) {
  if (screen < 0 || screen > static_cast<int>(Screen::kOptions)) return;
  screen_ = static_cast<Screen>(screen);
  build_menu();
  menu_.set_selected(0);  // like set_screen, but without building previews (no GL)
}

void FrontEndScene::layout_rows(const PanelLayout& layout) {
  const float pad = gui_.theme().padding;
  const int rows = menu_.size();
  row_rects_.assign(static_cast<size_t>(std::max(0, rows)), RowRect{});
  const float bx = layout.x + pad * 0.6f;
  const float bw = layout.w - pad * 1.2f;
  float y = layout.y + layout.title_h;
  for (int i = 0; i < rows; ++i) {
    row_rects_[static_cast<size_t>(i)] = RowRect{bx, y, bw, layout.button_h};
    y += layout.button_h + layout.button_gap;
  }
}

int FrontEndScene::hit_test_row(float vx, float vy) const {
  for (size_t i = 0; i < row_rects_.size(); ++i) {
    const RowRect& r = row_rects_[i];
    if (vx >= r.x && vx <= r.x + r.w && vy >= r.y && vy <= r.y + r.h) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void FrontEndScene::pointer_event(int drawable_width, int drawable_height, float x,
                                  float y, bool click, bool moved) {
  gui_.set_scale(std::max(1.0f, static_cast<float>(drawable_height) / 720.0f));
  gui_.begin(drawable_width, drawable_height);
  layout_rows(panel_layout());
  Controls controls;
  controls.mouse_x = x;
  controls.mouse_y = y;
  controls.mouse_click = click;
  controls.mouse_moved = moved;
  handle_mouse(controls);
}

void FrontEndScene::handle_mouse(const Controls& controls) {
  // An offline capture must be reproducible, and the pointer's resting position
  // is an artifact of the display server. Ignore it while a screenshot is being
  // taken so the keyboard-selected row is what ends up in the image.
  if (!screenshot_path_.empty()) return;
  // The row rectangles are filled in by render_overlay() and are in view units;
  // the cursor arrives in drawable pixels, so divide by the GUI scale.
  if (controls.mouse_x < 0.0f || row_rects_.empty() || menu_.empty()) return;
  const float s = std::max(1.0f, gui_.scale());
  const float vx = controls.mouse_x / s;
  const float vy = controls.mouse_y / s;

  const int row = hit_test_row(vx, vy);
  if (row < 0 || row >= menu_.size()) return;
  const ui::MenuItem& item = menu_.items()[static_cast<size_t>(row)];
  if (!item.enabled) return;  // disabled rows neither hover nor activate

  if (controls.mouse_click) {
    if (row != menu_.selected()) {
      menu_.set_selected(row);
      on_selection_changed();
    }
    if (item.has_values()) {
      // Click the left half of a value row to step back, the right half forward.
      const RowRect& r = row_rects_[static_cast<size_t>(row)];
      menu_.adjust(vx < r.x + r.w * 0.5f ? -1 : 1);
      apply_value(row);
    } else {
      activate();
    }
    return;
  }

  // Hover only on frames where the cursor actually moved, so a resting mouse
  // does not fight the keyboard selection.
  if (controls.mouse_moved && row != menu_.selected()) {
    menu_.set_selected(row);
    on_selection_changed();
  }
}

void FrontEndScene::go_back() {
  switch (screen_) {
    case Screen::kMain: next_ = "quit"; break;
    case Screen::kTracks:
    case Screen::kOpponent:
    case Screen::kOptions: set_screen(Screen::kMain); break;
    case Screen::kCar:
      set_screen(car_target_ == CarTarget::kOpponent ? Screen::kOpponent : Screen::kMain);
      break;
  }
}

void FrontEndScene::cycle_car(int delta) {
  if (config_ == nullptr) return;
  const int n = static_cast<int>(cars_.size());
  if (n == 0) return;
  std::string& current = (car_target_ == CarTarget::kPlayer) ? config_->player_car
                                                            : config_->opponent_car;
  int index = 0;
  for (int i = 0; i < n; ++i) {
    if (cars_[static_cast<size_t>(i)] == current) {
      index = i;
      break;
    }
  }
  index = ((index + delta) % n + n) % n;
  current = cars_[static_cast<size_t>(index)];
  ensure_car_preview();
}

// The opponent choice is an index: 0 is "Race against the Clock" (no opponent,
// which the race scene represents with an empty opponent car) and 1..6 pick a
// driver, which also selects that driver's route in OPPn.PRE.
int FrontEndScene::current_opponent_index() const {
  if (config_ == nullptr || config_->opponent_car.empty()) return 0;
  return std::clamp(config_->opponent_type, 1, kOpponentSlots);
}

void FrontEndScene::set_opponent(int index) {
  if (config_ == nullptr) return;
  const int slots = kOpponentSlots + 1;
  index = ((index % slots) + slots) % slots;
  if (index == 0) {
    config_->opponent_car.clear();
  } else {
    config_->opponent_type = index;
    if (config_->opponent_car.empty()) config_->opponent_car = config_->player_car;
  }
  ensure_opponent_face();
}

void FrontEndScene::cycle_opponent(int delta) {
  set_opponent(current_opponent_index() + delta);
}

void FrontEndScene::activate() {
  const int row = menu_.selected();
  const ui::MenuItem* item = menu_.current();
  // A value row (drop-down) responds to Enter: step forward.
  if (item != nullptr && item->enabled && item->has_values()) {
    menu_.adjust(1);
    apply_value(row);
    return;
  }
  switch (screen_) {
    case Screen::kMain:
      switch (row) {
        case 0: next_ = "drive"; break;  // config already holds the selections
        case 1: set_screen(Screen::kTracks); break;
        case 2:
          car_target_ = CarTarget::kPlayer;
          set_screen(Screen::kCar);
          break;
        case 3: set_screen(Screen::kOpponent); break;
        case 4: set_screen(Screen::kOptions); break;
        default: next_ = "quit"; break;
      }
      break;

    case Screen::kTracks: {
      const int last = menu_.size() - 1;
      if (row == last) {
        set_screen(Screen::kMain);  // "Main Menu"
      }
      // The value row at 0 is handled above; the disabled "New / Edit Tracks"
      // row does nothing.
      break;
    }

    case Screen::kCar: {
      switch (row) {
        case 0: go_back(); break;       // Done
        case 1: cycle_car(1); break;    // Next
        case 2: cycle_car(-1); break;   // Last
        default: break;                 // transmission / colour are placeholders
      }
      break;
    }

    case Screen::kOpponent: {
      switch (row) {
        case 0: go_back(); break;           // Done
        case 1: cycle_opponent(-1); break;  // Last
        case 2: cycle_opponent(1); break;   // Next
        case 3: set_opponent(0); break;     // Clock = race against the clock
        case 4:                             // Car: edit the opponent's car
          car_target_ = CarTarget::kOpponent;
          set_screen(Screen::kCar);
          break;
        default: go_back(); break;
      }
      break;
    }

    case Screen::kOptions: {
      if (row == menu_.size() - 1) go_back();  // "Main Menu"
      break;
    }
  }
}

void FrontEndScene::apply_value(int row) {
  if (config_ == nullptr) return;
  const std::string value = menu_.current_value();
  if (value.empty()) return;

  if (screen_ == Screen::kTracks) {
    if (row == 0) {
      config_->last_track = value + ".TRK";
      ensure_track_preview();
    }
    return;
  }

  if (screen_ != Screen::kOptions) return;
  const bool on = (value == "On");
  switch (row) {
    case 0: {  // Resolution
      const size_t x = value.find('x');
      if (x == std::string::npos) return;
      config_->width = std::atoi(value.substr(0, x).c_str());
      config_->height = std::atoi(value.substr(x + 1).c_str());
      break;
    }
    case 1: config_->fullscreen = on; break;
    case 2:  // Wheel Scale
      config_->wheel_scale = (value == "1.00x")   ? 1.00f
                             : (value == "1.25x") ? 1.25f
                             : (value == "1.50x") ? 1.50f
                                                  : 1.75f;
      break;
    case 3: config_->pattern_fill = on; break;
    case 4: config_->show_fps = on; break;
    default: break;
  }
}

void FrontEndScene::ensure_demo() {
  if (demo_ || demo_tried_) return;
  demo_tried_ = true;

  std::string demo_track = kDemoTrack;
  if (!file_readable(path_join(asset_dir_, demo_track)) && !tracks_.empty()) {
    demo_track = tracks_.front();
  }
  Track track = Track::load(path_join(asset_dir_, demo_track));
  auto demo = std::make_unique<DriveScene>(std::move(track), asset_dir_, kDemoCar, "", 5,
                                           config_ ? config_->fov_deg : 80.0f,
                                           config_ ? config_->wheel_scale : 1.25f);
  demo->set_demo(true);
  demo->set_camera_mode(1);  // chase camera
  if (!demo->on_enter()) {
    S3D_WARN("front-end: attract demo failed to start");
    return;
  }
  S3D_INFO("front-end: attract demo on %s with %s", demo_track.c_str(), kDemoCar);
  demo_ = std::move(demo);
}

void FrontEndScene::ensure_track_preview() {
  const int n = static_cast<int>(tracks_.size());
  if (n == 0) return;
  const std::string selected = (config_ != nullptr) ? config_->last_track : std::string();
  int row = 0;
  for (int i = 0; i < n; ++i) {
    if (tracks_[static_cast<size_t>(i)] == selected) {
      row = i;
      break;
    }
  }
  if (track_preview_ && row == track_preview_index_) return;

  Track track = Track::load(path_join(asset_dir_, tracks_[static_cast<size_t>(row)]));
  auto scene = std::make_unique<DebugTrackScene>(std::move(track),
                                                 tracks_[static_cast<size_t>(row)],
                                                 asset_dir_);
  scene->set_top_down(true);
  if (!scene->on_enter()) return;
  track_preview_ = std::move(scene);
  track_preview_index_ = row;
}

void FrontEndScene::ensure_car_preview() {
  // The Car screen has no model list, so the preview follows the configured
  // car (player or opponent) rather than the menu cursor.
  const int n = static_cast<int>(cars_.size());
  if (n == 0) return;
  const std::string selected =
      (config_ == nullptr) ? std::string()
      : (car_target_ == CarTarget::kPlayer ? config_->player_car
                                           : config_->opponent_car);
  int row = 0;
  for (int i = 0; i < n; ++i) {
    if (cars_[static_cast<size_t>(i)] == selected) {
      row = i;
      break;
    }
  }
  if (car_preview_valid_ && row == car_preview_index_) return;

  ShapeLibrary lib;
  if (!world::load_car_archive(asset_dir_, cars_[static_cast<size_t>(row)], &lib)) {
    car_preview_valid_ = false;
    car_preview_index_ = -1;
    return;
  }
  const glm::vec4 backlight = have_palette_ ? world::material_color(0x2E, palette_)
                                            : glm::vec4(0.67f, 0.0f, 0.0f, 1.0f);
  const Palette* pal = have_palette_ ? &palette_ : nullptr;
  MeshBuilder mb;
  world::append_car_model(
      mb, lib, glm::mat4(1.0f),
      [pal, backlight](uint8_t paint) {
        if (pal != nullptr) return world::car_primitive_color(paint, *pal, backlight);
        return paint == world::kCarBacklightPaint ? backlight
                                                  : world::track_paint_color(paint);
      },
      0, nullptr, config_ ? config_->wheel_scale : 1.0f);
  car_preview_mesh_ = mb.to_mesh();
  car_preview_valid_ = true;
  car_preview_index_ = row;

  // Frame the camera on the body's own bounds: the archive stores the racing
  // body at racing scale (a little over one grid square long), so a fixed
  // viewing distance would leave the car either a speck or off-screen.
  glm::vec3 lo(1e30f), hi(-1e30f);
  for (const Vertex& v : mb.vertices()) {
    lo = glm::min(lo, v.position);
    hi = glm::max(hi, v.position);
  }
  const glm::vec3 centre = (lo + hi) * 0.5f;
  const float radius = std::max(1.0f, glm::length(hi - lo) * 0.5f);
  const float fov = 32.0f;
  const float dist = radius / std::tan(glm::radians(fov * 0.5f)) * 1.8f;
  const glm::vec3 dir = glm::normalize(glm::vec3(-0.62f, 0.42f, -0.66f));
  car_preview_focus_ = centre;
  car_preview_cam_.fov_deg = fov;
  car_preview_cam_.position = centre + dir * dist;
  // Aim a touch under the body so the perspective's larger near end does not
  // push the silhouette below the middle of the free area.
  aim_at(car_preview_cam_, centre - glm::vec3(0.0f, radius * 0.12f, 0.0f));
}

void FrontEndScene::ensure_opponent_face() {
  // The face follows the chosen opponent (default: Race against the Clock),
  // not the menu cursor.
  const int want = current_opponent_index();
  if (face_texture_.valid() && want == face_index_) return;
  if (!have_palette_) return;

  Sprite2D sprite;
  const std::string id = "opp" + std::to_string(want);
  if (!load_sprite2d(path_join(asset_dir_, "SDOSEL.PVS"), id, &sprite)) {
    face_index_ = -1;
    return;
  }
  std::vector<uint8_t> rgba(sprite.pixels.size() * 4, 0);
  for (size_t p = 0; p < sprite.pixels.size(); ++p) {
    const uint8_t idx = sprite.pixels[p];
    if (idx == 0xFF) continue;  // transparent
    const uint8_t* c = palette_.rgb8[idx];
    rgba[p * 4 + 0] = c[0];
    rgba[p * 4 + 1] = c[1];
    rgba[p * 4 + 2] = c[2];
    rgba[p * 4 + 3] = 255;
  }
  face_texture_.upload_rgba(sprite.width, sprite.height, rgba.data());
  face_sprite_ = std::move(sprite);
  face_index_ = want;
}

void FrontEndScene::update(double dt, const Controls& controls) {
  handle_input(controls);
  if (!next_.empty()) return;  // leaving: stop simulating

  // Keep the attract demo running behind the main screen. It gets no input (it
  // is a self-driving autopilot), so passing empty controls keeps Escape/Enter
  // from reaching the demo's own handlers.
  if (screen_ == Screen::kMain) {
    ensure_demo();
    if (demo_) demo_->update(dt, Controls{});
  }
  preview_angle_ += static_cast<float>(dt) * 30.0f;  // car turntable
  if (preview_angle_ > 360.0f) preview_angle_ -= 360.0f;
}

void FrontEndScene::draw_block_background(int width, int height) {
  if (!bg_texture_.valid() || width <= 0 || height <= 0) return;
  const float tiles_x = static_cast<float>(width) / (ui::kBlockTilePixels);
  const float tiles_y = static_cast<float>(height) / (ui::kBlockTilePixels);
  std::vector<Vertex> quad;
  Vertex v;
  v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  const glm::vec3 pos[6] = {{0.0f, 0.0f, 0.0f},
                            {static_cast<float>(width), 0.0f, 0.0f},
                            {static_cast<float>(width), static_cast<float>(height), 0.0f},
                            {0.0f, 0.0f, 0.0f},
                            {static_cast<float>(width), static_cast<float>(height), 0.0f},
                            {0.0f, static_cast<float>(height), 0.0f}};
  const glm::vec2 uv[6] = {{0.0f, 0.0f}, {tiles_x, 0.0f}, {tiles_x, tiles_y},
                           {0.0f, 0.0f}, {tiles_x, tiles_y}, {0.0f, tiles_y}};
  for (int i = 0; i < 6; ++i) {
    v.position = pos[i];
    v.uv = uv[i];
    quad.push_back(v);
  }
  gui_.sprites().draw(bg_texture_, quad, width, height);
}

void FrontEndScene::draw_car_preview(Renderer& renderer, int width, int height) {
  (void)width;
  (void)height;
  if (!car_preview_valid_) return;
  renderer.set_polygon_offset(0.0f, 0.0f);

  // Slide the camera sideways so the car sits in the free area beside the menu
  // panel instead of behind it. `right()` is screen-right (the view mirrors GL's
  // X axis), so moving the camera that way shifts the car left on screen.
  Camera cam = car_preview_cam_;
  const float vw = static_cast<float>(gui_.view_width());
  const float target_vx = preview_center_vx(panel_layout());
  const float ndc = vw > 0.0f ? (2.0f * target_vx / vw - 1.0f) : 0.0f;
  const float dist = glm::length(cam.position - car_preview_focus_);
  cam.position +=
      cam.right() * (-ndc * dist * std::tan(glm::radians(cam.fov_deg * 0.5f)));

  const glm::mat4 model =
      glm::rotate(glm::mat4(1.0f), glm::radians(preview_angle_), glm::vec3(0.0f, 1.0f, 0.0f));
  renderer.draw_world(car_preview_mesh_, cam, model);
}

void FrontEndScene::draw_opponent_face(int width, int height) {
  if (!face_texture_.valid()) return;
  const PanelLayout layout = panel_layout();
  const float center_x = preview_center_vx(layout) * gui_.scale();
  const float target_h = static_cast<float>(height) * 0.62f;
  const float scale = target_h / static_cast<float>(face_sprite_.height);
  const float w = static_cast<float>(face_sprite_.width) * scale;
  const float h = static_cast<float>(face_sprite_.height) * scale;
  const float x = center_x - w * 0.5f;
  const float y = (static_cast<float>(height) - h) * 0.5f;
  face_quad_ = make_tex_quad(x, y, w, h);
  gui_.sprites().draw(face_texture_, face_quad_, width, height, true);
}

bool FrontEndScene::panel_on_right() const {
  // The preview screens keep the panel out of the way; the plain screens centre
  // it like a conventional menu.
  return screen_ == Screen::kTracks || screen_ == Screen::kCar ||
         screen_ == Screen::kOpponent;
}

FrontEndScene::PanelLayout FrontEndScene::panel_layout() const {
  const ui::GuiTheme& theme = gui_.theme();
  const float vw = static_cast<float>(gui_.view_width());
  const float vh = static_cast<float>(gui_.view_height());
  const float pad = theme.padding;
  const float margin = 24.0f;
  const int rows = menu_.size();

  PanelLayout p;
  p.title_h = 58.0f;
  // Wide enough for the longest label + value pair at this font size
  // ("Patterned Surfaces" / "1280x720") without the two colliding. The Options
  // screen carries the widest value rows, so it gets a little more room.
  p.w = (screen_ == Screen::kOptions) ? 620.0f : 520.0f;

  // Shrink the rows when a long list would overflow the viewport (the Car
  // screen has enough rows to need this on a short window).
  const float avail = std::max(80.0f, vh - p.title_h - pad * 1.2f - margin * 2.0f);
  const float base = rows > 0 ? rows * theme.button_height + (rows - 1) * theme.button_gap
                              : 0.0f;
  const float k = (base > 0.0f && base > avail) ? avail / base : 1.0f;
  p.button_h = std::max(24.0f, theme.button_height * k);
  p.button_gap = std::max(3.0f, theme.button_gap * k);

  const float list_h = rows > 0 ? rows * p.button_h + (rows - 1) * p.button_gap : 0.0f;
  p.h = p.title_h + list_h + pad * 1.2f;
  p.y = std::max(12.0f, (vh - p.h) * 0.5f);
  p.x = panel_on_right() ? (vw - p.w - margin) : (vw - p.w) * 0.5f;
  return p;
}

float FrontEndScene::preview_center_vx(const PanelLayout& layout) const {
  if (!panel_on_right()) return static_cast<float>(gui_.view_width()) * 0.5f;
  // Centre of the free area to the left of the panel.
  return layout.x * 0.5f;
}

void FrontEndScene::render_overlay(int width, int height) {
  (void)width;
  (void)height;
  const PanelLayout layout = panel_layout();
  const float cx = layout.x + layout.w * 0.5f;

  if (screen_ == Screen::kMain) gui_.scrim();
  // The panel background must be laid down before its title: it is translucent,
  // so drawing it afterwards would tint the title text grey.
  gui_.panel(layout.x, layout.y, layout.w, layout.h);
  gui_.title_centered(screen_title(), cx, layout.y + 12.0f);

  const float pad = gui_.theme().padding;
  const int rows = menu_.size();
  layout_rows(layout);
  float y = layout.y + layout.title_h;
  for (int i = 0; i < rows; ++i) {
    const ui::MenuItem& item = menu_.items()[static_cast<size_t>(i)];
    const bool focused = (i == menu_.selected());
    const float bx = layout.x + pad * 0.6f;
    const float bw = layout.w - pad * 1.2f;
    if (item.has_values()) {
      const int vi = item.value_index;
      const std::string value =
          (vi >= 0 && vi < static_cast<int>(item.values.size())) ? item.values[static_cast<size_t>(vi)]
                                                                 : std::string();
      gui_.value_button(bx, y, bw, layout.button_h, item.label, value, focused, item.enabled);
    } else {
      gui_.button(bx, y, bw, layout.button_h, item.label, focused, item.enabled);
    }
    y += layout.button_h + layout.button_gap;
  }

  gui_.flush();
}

void FrontEndScene::render(Renderer& renderer, int width, int height) {
  // Establish the GUI's view units first: the previews below position
  // themselves relative to the menu panel.
  gui_.set_scale(std::max(1.0f, static_cast<float>(height) / 720.0f));
  gui_.begin(width, height);

  switch (screen_) {
    case Screen::kMain:
      if (demo_) demo_->render(renderer, width, height);
      break;
    case Screen::kTracks:
      draw_block_background(width, height);
      if (track_preview_) {
        // Keep the map clear of the menu panel on the right.
        track_preview_->set_overview_width(panel_layout().x * gui_.scale());
        track_preview_->set_overview_rotated(true);
        track_preview_->render(renderer, width, height);
      }
      break;
    case Screen::kCar:
      draw_block_background(width, height);
      draw_car_preview(renderer, width, height);
      break;
    case Screen::kOpponent:
      draw_block_background(width, height);
      draw_opponent_face(width, height);
      break;
    case Screen::kOptions:
      draw_block_background(width, height);
      break;
  }
  render_overlay(width, height);

  if (!screenshot_path_.empty()) {
    ++frame_index_;
    if (frame_index_ >= screenshot_frame_) {
      capture_frame_png(screenshot_path_, width, height);
      screenshot_path_.clear();
      next_ = "quit";
    }
  }
}

void FrontEndScene::request_screenshot(std::string path, int frames) {
  screenshot_path_ = std::move(path);
  screenshot_frame_ = frames > 0 ? frames : 1;
}

}  // namespace s3d
