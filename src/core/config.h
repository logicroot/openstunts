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
  // The player car id (the original's gameconfig.game_playercarid), e.g. "COUN"
  // (Countach) or "VETT" (Vetty). Selects CAR<ID>.RES / ST<ID>.P3S / STDA<ID>.PVS.
  std::string player_car = "COUN";
  // The opponent car id (gameconfig.game_opponentcarid); empty = race without an
  // opponent. Selects the same file set as the player car.
  std::string opponent_car;
  // The opponent driving style (gameconfig.game_opponenttype), 1..6: selects
  // the OPPn.PRE sped table that sets its per-element target speeds.
  int opponent_type = 1;

  // Returns the default config file path:
  //   Linux/macOS: $XDG_CONFIG_HOME/stunts3d/config.ini or ~/.config/stunts3d/config.ini
  //   Windows:     %APPDATA%/stunts3d/config.ini
  static std::string default_path();

  // Loads from `path` (or default_path() if empty). Missing file is not an error.
  static Config load(const std::string& path = {});
  bool save(const std::string& path = {}) const;
};

}  // namespace s3d
