#include <cstdlib>
#include <memory>
#include <string>

#include "assets/trk.h"
#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"
#include "game/application.h"
#include "game/debug_track_scene.h"
#include "game/drive_scene.h"

using namespace s3d;

int main(int argc, char** argv) {
  std::string assets_arg;
  std::string track_arg;
  std::string config_path;
  std::string car_arg;
  std::string opp_car_arg;
  int opp_type_arg = 0;
  std::string screenshot_path;
  int screenshot_frame = 0;
  int camera_mode = 0;
  float wheel_scale_arg = 0.0f;
  bool top_down = false;
  bool drive = false;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--assets" && i + 1 < argc) assets_arg = argv[++i];
    else if (a == "--track" && i + 1 < argc) track_arg = argv[++i];
    else if (a == "--config" && i + 1 < argc) config_path = argv[++i];
    else if (a == "--car" && i + 1 < argc) car_arg = argv[++i];
    else if (a == "--opp-car" && i + 1 < argc) opp_car_arg = argv[++i];
    else if (a == "--opp" && i + 1 < argc) opp_type_arg = std::atoi(argv[++i]);
    else if (a == "--screenshot" && i + 1 < argc) {
      screenshot_path = argv[++i];
      if (screenshot_frame == 0) screenshot_frame = 3;
    } else if (a == "--frames" && i + 1 < argc) {
      screenshot_frame = std::atoi(argv[++i]);
    } else if (a == "--camera" && i + 1 < argc) {
      camera_mode = std::atoi(argv[++i]);
    } else if (a == "--wheel-scale" && i + 1 < argc) {
      wheel_scale_arg = static_cast<float>(std::atof(argv[++i]));
    } else if (a == "--drive") {
      drive = true;
    } else if (a == "--top") {
      top_down = true;
    } else if (a == "--help" || a == "-h") {
      std::printf(
          "Stunts3D — 3D reimplementation of Stunts\n"
          "usage: stunts3d [--assets DIR] [--track FILE.trk] [--config FILE]\n"
          "                [--car ID] [--opp-car ID] [--opp N] [--screenshot out.png]\n"
          "                [--frames N] [--camera N] [--wheel-scale F] [--top] [--drive]\n");
      return 0;
    } else {
      S3D_WARN("unknown argument: %s", a.c_str());
    }
  }

  Config config = Config::load(config_path);
  if (!assets_arg.empty()) config.asset_dir = assets_arg;
  if (!car_arg.empty()) config.player_car = car_arg;
  if (!opp_car_arg.empty()) config.opponent_car = opp_car_arg;
  if (opp_type_arg > 0) config.opponent_type = opp_type_arg;
  if (wheel_scale_arg > 0.0f) config.wheel_scale = wheel_scale_arg;

  const std::string asset_dir = find_asset_dir(config.asset_dir);
  const std::string track_name = !track_arg.empty() ? track_arg : config.last_track;
  std::string track_path = track_name;
  if (track_path.find('/') == std::string::npos && track_path.find('\\') == std::string::npos) {
    track_path = path_join(asset_dir, track_name);
  }
  S3D_INFO("assets dir : %s", asset_dir.empty() ? "(not found)" : asset_dir.c_str());
  S3D_INFO("track file : %s", track_path.c_str());

  Track track = Track::load(track_path);

  Application app;
  if (!app.init(config, asset_dir)) return 1;

  if (drive) {
    auto scene = std::make_unique<DriveScene>(std::move(track), asset_dir,
                                             config.player_car, config.opponent_car,
                                             config.opponent_type, config.fov_deg,
                                             config.wheel_scale);
    scene->set_camera_mode(camera_mode);
    if (!screenshot_path.empty()) scene->request_screenshot(screenshot_path, screenshot_frame);
    app.set_scene(std::move(scene));
  } else {
    auto scene = std::make_unique<DebugTrackScene>(std::move(track), track_path, asset_dir);
    scene->set_top_down(top_down);
    if (!screenshot_path.empty()) scene->request_screenshot(screenshot_path, screenshot_frame);
    app.set_scene(std::move(scene));
  }
  app.run();

  return 0;
}
