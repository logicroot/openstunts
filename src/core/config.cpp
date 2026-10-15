#include "core/config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "core/log.h"
#include "core/paths.h"

namespace s3d {

namespace {

std::string config_dir() {
#ifdef _WIN32
  if (const char* appdata = std::getenv("APPDATA")) {
    return std::string(appdata) + "/stunts3d";
  }
  return "stunts3d";
#else
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
    return std::string(xdg) + "/stunts3d";
  }
  if (const char* home = std::getenv("HOME")) {
    return std::string(home) + "/.config/stunts3d";
  }
  return "stunts3d";
#endif
}

bool parse_bool(const std::string& v) { return v == "1" || v == "true" || v == "yes"; }

}  // namespace

std::string Config::default_path() { return config_dir() + "/config.ini"; }

Config Config::load(const std::string& path) {
  Config c;
  const std::string p = path.empty() ? default_path() : path;
  std::ifstream in(p);
  if (!in) return c;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    const size_t eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    auto trim = [](std::string& s) {
      const char* ws = " \t\r\n";
      s.erase(0, s.find_first_not_of(ws));
      s.erase(s.find_last_not_of(ws) + 1);
    };
    trim(key);
    trim(val);

    if (key == "width") c.width = std::atoi(val.c_str());
    else if (key == "height") c.height = std::atoi(val.c_str());
    else if (key == "vsync") c.vsync = parse_bool(val);
    else if (key == "fullscreen") c.fullscreen = parse_bool(val);
    else if (key == "highdpi") c.highdpi = parse_bool(val);
    else if (key == "mouse_sensitivity") c.mouse_sensitivity = std::atof(val.c_str());
    else if (key == "fov_deg") c.fov_deg = static_cast<float>(std::atof(val.c_str()));
    else if (key == "wheel_scale") c.wheel_scale = static_cast<float>(std::atof(val.c_str()));
    else if (key == "show_fps") c.show_fps = parse_bool(val);
    else if (key == "asset_dir") c.asset_dir = val;
    else if (key == "last_track") c.last_track = val;
    else if (key == "player_car") c.player_car = val;
    else if (key == "opponent_car") c.opponent_car = val;
    else if (key == "opponent_type") c.opponent_type = std::atoi(val.c_str());
    else if (key == "pattern_fill") c.pattern_fill = parse_bool(val);
  }
  S3D_INFO("config: loaded %s", p.c_str());
  return c;
}

bool Config::save(const std::string& path) const {
  const std::string p = path.empty() ? default_path() : path;
  // Ensure the directory exists.
  const size_t slash = p.find_last_of("/\\");
  if (slash != std::string::npos) {
    const std::string dir = p.substr(0, slash);
#ifdef _WIN32
    std::string cmd = "mkdir \"" + dir + "\" >nul 2>nul";
#else
    std::string cmd = "mkdir -p \"" + dir + "\"";
#endif
    if (std::system(cmd.c_str()) != 0) {
      // Non-fatal; the ofstream below will report a real failure.
    }
  }
  std::ofstream out(p);
  if (!out) {
    S3D_WARN("config: cannot write %s", p.c_str());
    return false;
  }
  out << "# Stunts3D settings\n";
  out << "width = " << width << "\n";
  out << "height = " << height << "\n";
  out << "vsync = " << (vsync ? 1 : 0) << "\n";
  out << "fullscreen = " << (fullscreen ? 1 : 0) << "\n";
  out << "highdpi = " << (highdpi ? 1 : 0) << "\n";
  out << "mouse_sensitivity = " << mouse_sensitivity << "\n";
  out << "fov_deg = " << fov_deg << "\n";
  out << "wheel_scale = " << wheel_scale << "\n";
  out << "show_fps = " << (show_fps ? 1 : 0) << "\n";
  out << "asset_dir = " << asset_dir << "\n";
  out << "last_track = " << last_track << "\n";
  out << "player_car = " << player_car << "\n";
  out << "opponent_car = " << opponent_car << "\n";
  out << "opponent_type = " << opponent_type << "\n";
  out << "pattern_fill = " << (pattern_fill ? 1 : 0) << "\n";
  return true;
}

}  // namespace s3d
