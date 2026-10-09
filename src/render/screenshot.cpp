#include "render/screenshot.h"

#include <vector>

#include "core/log.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "render/gl.h"

namespace s3d {

bool capture_frame_png(const std::string& path, int drawable_width, int drawable_height) {
  if (drawable_width <= 0 || drawable_height <= 0) return false;

  std::vector<unsigned char> pixels(static_cast<size_t>(drawable_width) *
                                    static_cast<size_t>(drawable_height) * 4);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, drawable_width, drawable_height, GL_RGBA, GL_UNSIGNED_BYTE,
               pixels.data());

  // OpenGL's origin is bottom-left; PNG wants top-left. Flip rows.
  const size_t row = static_cast<size_t>(drawable_width) * 4;
  std::vector<unsigned char> tmp(row);
  for (int y = 0; y < drawable_height / 2; ++y) {
    unsigned char* a = pixels.data() + static_cast<size_t>(y) * row;
    unsigned char* b = pixels.data() + static_cast<size_t>(drawable_height - 1 - y) * row;
    std::copy(a, a + row, tmp.data());
    std::copy(b, b + row, a);
    std::copy(tmp.begin(), tmp.end(), b);
  }

  const int ok = stbi_write_png(path.c_str(), drawable_width, drawable_height, 4,
                                pixels.data(), static_cast<int>(row));
  if (ok == 0) {
    S3D_ERROR("screenshot: failed to write '%s'", path.c_str());
    return false;
  }
  S3D_INFO("screenshot: wrote %s (%dx%d)", path.c_str(), drawable_width, drawable_height);
  return true;
}

}  // namespace s3d
