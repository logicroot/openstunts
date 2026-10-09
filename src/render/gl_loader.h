#pragma once
// Minimal OpenGL 3.3 core function loader.
//
// Rationale: Windows' opengl32.dll only exports OpenGL 1.1 entry points, so all
// modern functions must be resolved at runtime. SDL_GL_GetProcAddress() works on
// Windows, Linux and macOS, which keeps this project dependency-free (no GLAD,
// no GLEW).
//
// OpenGL 1.1 entry points (glEnable, glGenTextures, glDrawArrays, ...) are
// declared and linked directly; everything from 1.2 upwards is loaded here.

#include "render/gl_api.h"

namespace s3d::gl {

// Typedefs for the runtime-loaded entry points (OpenGL 1.3+).
typedef void(S3D_GLAPI* PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void(S3D_GLAPI* PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void(S3D_GLAPI* PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void(S3D_GLAPI* PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size,
                                            const void* data, GLenum usage);
typedef void(S3D_GLAPI* PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset,
                                               GLsizeiptr size, const void* data);
typedef void(S3D_GLAPI* PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void(S3D_GLAPI* PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void(S3D_GLAPI* PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void(S3D_GLAPI* PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void(S3D_GLAPI* PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void(S3D_GLAPI* PFNGLDISABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void(S3D_GLAPI* PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type,
                                                     GLboolean normalized, GLsizei stride,
                                                     const void* pointer);
typedef GLuint(S3D_GLAPI* PFNGLCREATESHADERPROC)(GLenum type);
typedef void(S3D_GLAPI* PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count,
                                              const GLchar* const* string,
                                              const GLint* length);
typedef void(S3D_GLAPI* PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void(S3D_GLAPI* PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef void(S3D_GLAPI* PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize,
                                                  GLsizei* length, GLchar* infoLog);
typedef GLuint(S3D_GLAPI* PFNGLCREATEPROGRAMPROC)(void);
typedef void(S3D_GLAPI* PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void(S3D_GLAPI* PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void(S3D_GLAPI* PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void(S3D_GLAPI* PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize,
                                                   GLsizei* length, GLchar* infoLog);
typedef void(S3D_GLAPI* PFNGLDELETESHADERPROC)(GLuint shader);
typedef void(S3D_GLAPI* PFNGLDELETEPROGRAMPROC)(GLuint program);
typedef void(S3D_GLAPI* PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint(S3D_GLAPI* PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void(S3D_GLAPI* PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void(S3D_GLAPI* PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void(S3D_GLAPI* PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void(S3D_GLAPI* PFNGLUNIFORM3FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void(S3D_GLAPI* PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void(S3D_GLAPI* PFNGLUNIFORMMATRIX3FVPROC)(GLint location, GLsizei count,
                                                  GLboolean transpose, const GLfloat* value);
typedef void(S3D_GLAPI* PFNGLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count,
                                                  GLboolean transpose, const GLfloat* value);
typedef void(S3D_GLAPI* PFNGLGENERATEMIPMAPPROC)(GLenum target);
typedef void(S3D_GLAPI* PFNGLBLENDFUNCSEPARATEPROC)(GLenum sfactorRGB, GLenum dfactorRGB,
                                                   GLenum sfactorAlpha, GLenum dfactorAlpha);

// List of functions to load. Format: X(name, PFN type)
#define S3D_GL_FUNCTIONS(X)                                              \
  X(glActiveTexture, PFNGLACTIVETEXTUREPROC)                            \
  X(glGenBuffers, PFNGLGENBUFFERSPROC)                                  \
  X(glBindBuffer, PFNGLBINDBUFFERPROC)                                  \
  X(glBufferData, PFNGLBUFFERDATAPROC)                                  \
  X(glBufferSubData, PFNGLBUFFERSUBDATAPROC)                            \
  X(glDeleteBuffers, PFNGLDELETEBUFFERSPROC)                            \
  X(glGenVertexArrays, PFNGLGENVERTEXARRAYSPROC)                        \
  X(glBindVertexArray, PFNGLBINDVERTEXARRAYPROC)                        \
  X(glDeleteVertexArrays, PFNGLDELETEVERTEXARRAYSPROC)                  \
  X(glEnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC)        \
  X(glDisableVertexAttribArray, PFNGLDISABLEVERTEXATTRIBARRAYPROC)      \
  X(glVertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC)                \
  X(glCreateShader, PFNGLCREATESHADERPROC)                              \
  X(glShaderSource, PFNGLSHADERSOURCEPROC)                              \
  X(glCompileShader, PFNGLCOMPILESHADERPROC)                            \
  X(glGetShaderiv, PFNGLGETSHADERIVPROC)                               \
  X(glGetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC)                      \
  X(glCreateProgram, PFNGLCREATEPROGRAMPROC)                            \
  X(glAttachShader, PFNGLATTACHSHADERPROC)                              \
  X(glLinkProgram, PFNGLLINKPROGRAMPROC)                                \
  X(glGetProgramiv, PFNGLGETPROGRAMIVPROC)                             \
  X(glGetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC)                    \
  X(glDeleteShader, PFNGLDELETESHADERPROC)                              \
  X(glDeleteProgram, PFNGLDELETEPROGRAMPROC)                            \
  X(glUseProgram, PFNGLUSEPROGRAMPROC)                                  \
  X(glGetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC)                  \
  X(glUniform1i, PFNGLUNIFORM1IPROC)                                    \
  X(glUniform1f, PFNGLUNIFORM1FPROC)                                    \
  X(glUniform2f, PFNGLUNIFORM2FPROC)                                    \
  X(glUniform3f, PFNGLUNIFORM3FPROC)                                    \
  X(glUniform4f, PFNGLUNIFORM4FPROC)                                    \
  X(glUniformMatrix3fv, PFNGLUNIFORMMATRIX3FVPROC)                      \
  X(glUniformMatrix4fv, PFNGLUNIFORMMATRIX4FVPROC)                      \
  X(glGenerateMipmap, PFNGLGENERATEMIPMAPPROC)                          \
  X(glBlendFuncSeparate, PFNGLBLENDFUNCSEPARATEPROC)

#define S3D_GL_DECLARE(name, type) extern type name;
S3D_GL_FUNCTIONS(S3D_GL_DECLARE)
#undef S3D_GL_DECLARE

// Resolves every function above. Returns false (and logs) if any is missing.
bool load();

}  // namespace s3d::gl
