// Front-end menu structure tests.
//
// The screens list their options in a fixed order (the design calls for "Done"
// first on the Car/Opponent screens) and the track chooser is a value row that
// hides the ".TRK" extension. The scene reads the track/car files at
// construction, so a scratch asset directory makes these checks hermetic;
// nothing here touches GL (only on_enter/init do).

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/config.h"
#include "game/frontend_scene.h"
#include "tiny_test.h"

using namespace s3d;
using namespace s3d_test;

namespace {

// A scratch asset directory holding the given (name, contents) files.
class ScratchDir {
 public:
  explicit ScratchDir(const std::string& tag) {
    path_ = std::filesystem::temp_directory_path() / ("s3d_frontend_" + tag);
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
    std::filesystem::create_directories(path_, ec);
  }
  ~ScratchDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  void add(const std::string& name, const std::string& contents = "") const {
    std::ofstream f(path_ / name, std::ios::binary);
    f << contents;
  }

  std::string str() const { return path_.string(); }

 private:
  std::filesystem::path path_;
};

std::string label_at(const ui::MenuModel& menu, int row) {
  const std::vector<ui::MenuItem>& items = menu.items();
  if (row < 0 || row >= static_cast<int>(items.size())) return "<out of range>";
  return items[static_cast<size_t>(row)].label;
}

bool enabled_at(const ui::MenuModel& menu, int row) {
  return menu.items()[static_cast<size_t>(row)].enabled;
}

}  // namespace

// The display name of a track file drops the extension, whatever its case.
TEST(frontend_track_label_hides_extension) {
  EXPECT_EQ(FrontEndScene::track_label("HELENS.TRK"), std::string("HELENS"));
  EXPECT_EQ(FrontEndScene::track_label("helens.trk"), std::string("helens"));
  EXPECT_EQ(FrontEndScene::track_label("SKIDS.TRK"), std::string("SKIDS"));
}

// A name without the extension is left alone.
TEST(frontend_track_label_without_extension) {
  EXPECT_EQ(FrontEndScene::track_label("HELENS"), std::string("HELENS"));
  EXPECT_EQ(FrontEndScene::track_label("TRK"), std::string("TRK"));
  EXPECT_EQ(FrontEndScene::track_label("backup.TRK.bak"), std::string("backup.TRK.bak"));
}

// Picking a drop-down entry appends the extension again: the label round-trips
// back to the file name on disk (the values in the menu are these labels).
TEST(frontend_track_label_round_trips_to_file) {
  const char* files[] = {"HELENS.TRK", "SKIDS.TRK", "MYTRACK.TRK"};
  for (const char* f : files) {
    EXPECT_EQ(FrontEndScene::track_label(f) + ".TRK", std::string(f));
  }
}

// The Tracks screen is a drop-down of *.TRK files (extension hidden) plus the
// disabled New/Edit placeholder and Main Menu; non-track files are ignored.
TEST(frontend_tracks_screen_lists_trk_files) {
  ScratchDir dir("tracks");
  dir.add("MYTRACK.TRK");
  dir.add("OTHER.TRK");
  dir.add("third.trk");   // lower-case extension still counts
  dir.add("README.TXT");  // not a track

  Config config;
  FrontEndScene scene(&config, "", dir.str());
  scene.select_screen(1);  // 1 = Tracks
  const ui::MenuModel& menu = scene.menu();

  EXPECT_EQ(menu.size(), 3);
  EXPECT_EQ(label_at(menu, 0), std::string("More Tracks"));
  EXPECT_TRUE(menu.items()[0].has_values());

  const std::vector<std::string>& values = menu.items()[0].values;
  EXPECT_EQ(values.size(), static_cast<size_t>(3));
  EXPECT_EQ(values[0], std::string("MYTRACK"));
  EXPECT_EQ(values[1], std::string("OTHER"));
  EXPECT_EQ(values[2], std::string("THIRD"));

  EXPECT_EQ(label_at(menu, 1), std::string("New / Edit Tracks"));
  EXPECT_TRUE(!enabled_at(menu, 1));
  EXPECT_EQ(label_at(menu, 2), std::string("Main Menu"));
  EXPECT_TRUE(enabled_at(menu, 2));
}

// The drop-down starts on the configured track.
TEST(frontend_tracks_screen_starts_on_current_track) {
  ScratchDir dir("tracks_current");
  dir.add("AAATRK.TRK");
  dir.add("BBTRK.TRK");

  Config config;
  config.last_track = "BBTRK.TRK";
  FrontEndScene scene(&config, "", dir.str());
  scene.select_screen(1);

  EXPECT_EQ(scene.menu().items()[0].value_index, 1);
}

// The Opponent screen mirrors the Car screen: "Done" is the first row and gets
// the focus, so Enter returns immediately.
TEST(frontend_opponent_done_is_first) {
  Config config;
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(3);  // 3 = Opponent
  const ui::MenuModel& menu = scene.menu();

  EXPECT_EQ(menu.size(), 5);
  EXPECT_EQ(label_at(menu, 0), std::string("Done"));
  EXPECT_EQ(label_at(menu, 1), std::string("Last"));
  EXPECT_EQ(label_at(menu, 2), std::string("Next"));
  EXPECT_EQ(label_at(menu, 3), std::string("Clock"));
  EXPECT_EQ(label_at(menu, 4), std::string("Car"));
  EXPECT_EQ(menu.selected(), 0);
}

// The Car screen keeps its five options in the documented order.
TEST(frontend_car_screen_order) {
  Config config;
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(2);  // 2 = Car
  const ui::MenuModel& menu = scene.menu();

  EXPECT_EQ(menu.size(), 5);
  EXPECT_EQ(label_at(menu, 0), std::string("Done"));
  EXPECT_EQ(label_at(menu, 1), std::string("Next"));
  EXPECT_EQ(label_at(menu, 2), std::string("Last"));
  EXPECT_EQ(label_at(menu, 3), std::string("Transmission"));
  EXPECT_EQ(label_at(menu, 4), std::string("Colour"));
  // Transmission and Colour are still display-only placeholders.
  EXPECT_TRUE(!enabled_at(menu, 3));
  EXPECT_TRUE(!enabled_at(menu, 4));
}

// --- Pointer (mouse) input ------------------------------------------------
//
// Rows are laid out from fixed metrics (title 58, button 36, gap 6, padding 16,
// panel margin 24), so a 1280x720 viewport puts every row at a predictable spot.
// These helpers mirror that arithmetic to build hit points; the tests then pin
// the pointer -> row mapping (the risky part of the mouse support) down.

namespace {

struct PanelPoint {
  float x_center = 0.0f;
  float x_left = 0.0f;   // left quarter of the row
  float x_right = 0.0f;  // right quarter of the row
  float y0 = 0.0f;       // centre of row 0
};

PanelPoint panel_points(float panel_w, bool on_right, int rows) {
  const float pad = 16.0f;
  const float margin = 24.0f;
  const float list_h = rows * 36.0f + (rows - 1) * 6.0f;
  const float h = 58.0f + list_h + pad * 1.2f;
  const float panel_y = std::max(12.0f, (720.0f - h) * 0.5f);
  const float panel_x = on_right ? (1280.0f - panel_w - margin) : (1280.0f - panel_w) * 0.5f;
  const float bx = panel_x + pad * 0.6f;
  const float bw = panel_w - pad * 1.2f;

  PanelPoint p;
  p.x_center = bx + bw * 0.5f;
  p.x_left = bx + bw * 0.25f;
  p.x_right = bx + bw * 0.75f;
  p.y0 = panel_y + 58.0f + 18.0f;
  return p;
}

float row_y(const PanelPoint& p, int row) { return p.y0 + static_cast<float>(row) * 42.0f; }

}  // namespace

// Moving the cursor onto a row focuses it (no click needed).
TEST(frontend_pointer_hover_focuses_row) {
  Config config;
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(3);  // Opponent: Done, Last, Next, Clock, Car
  const PanelPoint p = panel_points(520.0f, true, 5);

  scene.pointer_event(1280, 720, p.x_center, row_y(p, 1), /*click=*/false, /*moved=*/true);
  EXPECT_EQ(scene.menu().selected(), 1);
}

// A resting cursor (no motion event) leaves the keyboard selection alone.
TEST(frontend_pointer_stationary_cursor_ignored) {
  Config config;
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(3);
  const PanelPoint p = panel_points(520.0f, true, 5);

  scene.pointer_event(1280, 720, p.x_center, row_y(p, 2), /*click=*/false, /*moved=*/false);
  EXPECT_EQ(scene.menu().selected(), 0);
}

// Clicking a row activates it: "Done" (the first Opponent row) returns to Main.
TEST(frontend_pointer_click_activates_row) {
  Config config;
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(3);
  const PanelPoint p = panel_points(520.0f, true, 5);

  scene.pointer_event(1280, 720, p.x_center, row_y(p, 0), /*click=*/true, /*moved=*/true);
  EXPECT_EQ(scene.menu().size(), 6);  // back on the Main screen
}

// Clicking "Next" picks the first driver.
TEST(frontend_pointer_click_cycles_opponent) {
  Config config;
  config.player_car = "COUN";
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(3);
  const PanelPoint p = panel_points(520.0f, true, 5);

  scene.pointer_event(1280, 720, p.x_center, row_y(p, 2), /*click=*/true, /*moved=*/true);
  EXPECT_EQ(scene.current_opponent_index(), 1);
  EXPECT_EQ(config.opponent_car, std::string("COUN"));
}

// Disabled rows neither hover nor activate.
TEST(frontend_pointer_ignores_disabled_row) {
  Config config;
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(2);  // Car: Done, Next, Last, Transmission, Colour
  const PanelPoint p = panel_points(520.0f, true, 5);

  scene.pointer_event(1280, 720, p.x_center, row_y(p, 3), /*click=*/true, /*moved=*/true);
  EXPECT_EQ(scene.menu().selected(), 0);  // "Done" keeps the focus
}

// Value rows use the click's half: left steps back, right steps forward.
TEST(frontend_pointer_value_row_halves) {
  Config config;
  config.fullscreen = false;
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(4);  // Options, row 1 = Fullscreen (Off/On)
  const PanelPoint p = panel_points(620.0f, false, 7);

  scene.pointer_event(1280, 720, p.x_right, row_y(p, 1), /*click=*/true, /*moved=*/true);
  EXPECT_TRUE(config.fullscreen);

  scene.pointer_event(1280, 720, p.x_left, row_y(p, 1), /*click=*/true, /*moved=*/true);
  EXPECT_TRUE(!config.fullscreen);
}

// Offline captures must be reproducible: the pointer is ignored while a
// screenshot is pending (its resting position depends on the display server).
TEST(frontend_pointer_ignored_during_capture) {
  Config config;
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.select_screen(3);
  const PanelPoint p = panel_points(520.0f, true, 5);

  scene.request_screenshot("/tmp/s3d_unused_capture.png", 1);
  scene.pointer_event(1280, 720, p.x_center, row_y(p, 2), /*click=*/true, /*moved=*/true);
  EXPECT_EQ(scene.menu().selected(), 0);
}
