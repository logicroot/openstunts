#include "core/paths.h"

#include <cstdlib>
#include <fstream>
#include <vector>

#include <SDL2/SDL.h>

#include "core/log.h"

namespace s3d {

namespace {

bool has_file(const std::string& dir, const char* name) {
  std::ifstream f(path_join(dir, name), std::ios::binary);
  return static_cast<bool>(f);
}

// Directory holding the running executable (SDL adds a trailing separator).
std::string executable_dir() {
  char* base = SDL_GetBasePath();
  if (base == nullptr) return {};
  std::string dir(base);
  SDL_free(base);
  return dir;
}

}  // namespace

std::string path_join(const std::string& dir, const std::string& file) {
  if (dir.empty()) return file;
  const char last = dir.back();
  if (last == '/' || last == '\\') return dir + file;
  return dir + "/" + file;
}

bool looks_like_stunts_dir(const std::string& dir) {
  if (dir.empty()) return false;
  // Keyed on data files, not on STUNTS.COM: the repository ships the game data
  // but deliberately none of the DOS programs, and an original installation
  // directory is still recognised by the same two files.
  return has_file(dir, "DEFAULT.TRK") && has_file(dir, "GAME.PRE");
}

std::string find_asset_dir(const std::string& explicit_dir) {
  std::vector<std::string> candidates;
  if (!explicit_dir.empty()) candidates.push_back(explicit_dir);

  if (const char* env = std::getenv("STUNTS_DIR")) candidates.emplace_back(env);

  // The assets bundled with the build, copied next to the executable by CMake
  // (`assets/`), with a `../assets` fallback for binaries in a subdirectory
  // such as the test runner in `build/tests/`.
  const std::string exe_dir = executable_dir();
  if (!exe_dir.empty()) {
    candidates.push_back(path_join(exe_dir, "assets"));
    candidates.push_back(path_join(exe_dir, "../assets"));
  }
  candidates.push_back("assets");
  candidates.push_back("./assets");

  // A real Stunts installation still works as a fallback.
  if (const char* home = std::getenv("HOME")) {
    candidates.push_back(std::string(home) + "/stunts");
  }
  candidates.push_back("stunts");
  candidates.push_back("../stunts");

  for (const auto& c : candidates) {
    if (looks_like_stunts_dir(c)) {
      S3D_INFO("assets: using game directory '%s'", c.c_str());
      return c;
    }
  }
  S3D_WARN("assets: no Stunts directory found (searched %zu candidates)",
           candidates.size());
  return {};
}

}  // namespace s3d
