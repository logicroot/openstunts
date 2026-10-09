#include "render/gl_loader.h"

#include <SDL2/SDL.h>

#include "core/log.h"

namespace s3d::gl {

#define S3D_GL_DEFINE(name, type) type name = nullptr;
S3D_GL_FUNCTIONS(S3D_GL_DEFINE)
#undef S3D_GL_DEFINE

bool load() {
  bool ok = true;
#define S3D_GL_LOAD(name, type)                                        \
  do {                                                                 \
    name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name));       \
    if (name == nullptr) {                                             \
      S3D_ERROR("OpenGL: failed to load function '%s'", #name);        \
      ok = false;                                                      \
    }                                                                  \
  } while (0);
  S3D_GL_FUNCTIONS(S3D_GL_LOAD)
#undef S3D_GL_LOAD
  return ok;
}

}  // namespace s3d::gl
