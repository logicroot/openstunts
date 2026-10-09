#include "core/paths.h"

#include <cstdlib>
#include <fstream>
#include <vector>

#include "core/log.h"

namespace s3d {

std::string path_join(const std::string& dir, const std::string& file) {
  if (dir.empty()) return file;
  const char last = dir.back();
  if (last == '/' || last == '\\') return dir + file;
  return dir + "/" + file;
}

bool looks_like_stunts_dir(const std::string& dir) {
  if (dir.empty()) return false;
  std::ifstream f(path_join(dir, "STUNTS.COM"), std::ios::binary);
  if (!f) return false;
  std::ifstream t(path_join(dir, "DEFAULT.TRK"), std::ios::binary);
  return static_cast<bool>(t);
}

std::string find_asset_dir(const std::string& explicit_dir) {
  std::vector<std::string> candidates;
  if (!explicit_dir.empty()) candidates.push_back(explicit_dir);

  if (const char* env = std::getenv("STUNTS_DIR")) candidates.emplace_back(env);

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
