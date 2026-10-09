#pragma once
// Locating the original Stunts asset directory.

#include <string>

namespace s3d {

// Search order: explicit argument, $STUNTS_DIR, ~/stunts, ./stunts, ../stunts.
// Returns an empty string when nothing is found.
std::string find_asset_dir(const std::string& explicit_dir = {});

// Returns true if `dir` looks like a Stunts data directory.
bool looks_like_stunts_dir(const std::string& dir);

// Joins a directory and a file name with the platform separator.
std::string path_join(const std::string& dir, const std::string& file);

}  // namespace s3d
