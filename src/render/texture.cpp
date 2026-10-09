#include "render/texture.h"

#include <utility>

#include "core/log.h"
#include "render/gl.h"

namespace s3d {

Texture::~Texture() { destroy(); }

Texture::Texture(Texture&& other) noexcept { *this = std::move(other); }

Texture& Texture::operator=(Texture&& other) noexcept {
  if (this != &other) {
    destroy();
    id_ = other.id_;
    width_ = other.width_;
    height_ = other.height_;
    other.id_ = 0;
    other.width_ = other.height_ = 0;
  }
  return *this;
}

bool Texture::upload_rgba(int width, int height, const uint8_t* pixels,
                          const TextureDesc& desc) {
  destroy();
  if (width <= 0 || height <= 0 || pixels == nullptr) return false;

  glGenTextures(1, &id_);
  glBindTexture(GL_TEXTURE_2D, id_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels);

  const GLint min_filter =
      desc.mipmaps
          ? (desc.filter == TextureFilter::Nearest ? GL_NEAREST_MIPMAP_LINEAR
                                                   : GL_LINEAR_MIPMAP_LINEAR)
          : (desc.filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR);
  const GLint mag_filter = (desc.filter == TextureFilter::Nearest) ? GL_NEAREST : GL_LINEAR;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
  const GLint wrap = (desc.wrap == TextureWrap::Repeat) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

  if (desc.mipmaps) glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);

  width_ = width;
  height_ = height;
  return true;
}

void Texture::destroy() {
  if (id_ != 0) {
    glDeleteTextures(1, &id_);
    id_ = 0;
  }
  width_ = height_ = 0;
}

void Texture::bind(int unit) const {
  glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
  glBindTexture(GL_TEXTURE_2D, id_);
}

}  // namespace s3d
