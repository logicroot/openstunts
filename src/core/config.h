#pragma once
// Simple persistent settings (key = value text file).

#include <string>

namespace s3d {

struct Config {
  int width = 1280;
  int height = 720;
  bool vsync = true;
  bool fullscreen = false;
  bool highdpi = true;
  float mouse_sensitivity = 0.12f;
  float fov_deg = 65.0f;
  bool show_fps = true;
  std::string asset_dir;
  std::string last_track = "DEFAULT.TRK";

  // Returns the default config file path:
  //   Linux/macOS: $XDG_CONFIG_HOME/stunts3d/config.ini or ~/.config/stunts3d/config.ini
  //   Windows:     %APPDATA%/stunts3d/config.ini
  static std::string default_path();

  // Loads from `path` (or default_path() if empty). Missing file is not an error.
  static Config load(const std::string& path = {});
  bool save(const std::string& path = {}) const;
};

}  // namespace s3d
