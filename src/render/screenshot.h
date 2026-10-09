#pragma once
// Framebuffer capture helpers.

#include <string>

namespace s3d {

// Reads the current drawable area and writes it to a PNG file. Returns false on
// failure. Used for visual regression checks against the original game.
bool capture_frame_png(const std::string& path, int drawable_width, int drawable_height);

}  // namespace s3d
