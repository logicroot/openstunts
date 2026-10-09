#pragma once
// 2D texture wrapper (RGBA8).

#include <cstdint>
#include <vector>

namespace s3d {

enum class TextureFilter { Nearest, Linear };
enum class TextureWrap { Repeat, ClampToEdge };

struct TextureDesc {
  TextureFilter filter = TextureFilter::Nearest;  // retro-friendly default
  TextureWrap wrap = TextureWrap::ClampToEdge;
  bool mipmaps = false;
  bool srgb = false;
};

class Texture {
 public:
  Texture() = default;
  ~Texture();

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;
  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&& other) noexcept;

  // pixels: width*height*4 bytes, row 0 = top.
  bool upload_rgba(int width, int height, const uint8_t* pixels,
                   const TextureDesc& desc = {});
  void destroy();

  void bind(int unit = 0) const;

  int width() const { return width_; }
  int height() const { return height_; }
  bool valid() const { return id_ != 0; }
  unsigned id() const { return id_; }

 private:
  unsigned id_ = 0;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace s3d
