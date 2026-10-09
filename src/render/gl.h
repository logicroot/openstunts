#pragma once
// Pulls the loaded OpenGL functions into scope for convenience.

#include "render/gl_loader.h"

#define S3D_GL_USING(name, type) using ::s3d::gl::name;
S3D_GL_FUNCTIONS(S3D_GL_USING)
#undef S3D_GL_USING
