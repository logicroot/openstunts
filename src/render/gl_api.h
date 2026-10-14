#pragma once
// Self-contained OpenGL type/constant/entry-point declarations.
//
// We deliberately do NOT include any system GL header (SDL_opengl.h, GL/gl.h,
// ...) so that the runtime-loaded function pointers (see gl_loader.h) never
// conflict with link-time prototypes. OpenGL 1.1 entry points are declared
// here and linked directly (available from opengl32.dll / libGL / OpenGL.framework);
// anything newer is resolved at runtime through SDL_GL_GetProcAddress.

#ifdef _WIN32
#define S3D_GLAPI __stdcall
#else
#define S3D_GLAPI
#endif

extern "C" {

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef char GLchar;
typedef void GLvoid;
typedef long long GLintptr;
typedef long long GLsizeiptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0

#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000

#define GL_LINES 0x0001
#define GL_TRIANGLES 0x0004

#define GL_DEPTH_TEST 0x0B71
#define GL_LESS 0x0201
#define GL_LEQUAL 0x0203
#define GL_CULL_FACE 0x0B44
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_CCW 0x0901
#define GL_CW 0x0900
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_COLOR_WRITEMASK 0x0C23

#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_DOUBLE 0x140A

#define GL_POINTS 0x0000
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLE_STRIP 0x0005
#define GL_TRIANGLE_FAN 0x0006

#define GL_FILL 0x1B02
#define GL_LINE 0x1B01
#define GL_FRONT_AND_BACK 0x0408

#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_RGB 0x1907
#define GL_BGRA 0x80E1
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_MAX_TEXTURE_SIZE 0x0D33
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ACTIVE_UNIFORMS 0x8B86

#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_VIEWPORT 0x0BA2

// ---- OpenGL 1.1 entry points (linked directly) ------------------------------
extern void S3D_GLAPI glEnable(GLenum cap);
extern void S3D_GLAPI glDisable(GLenum cap);
extern void S3D_GLAPI glClear(GLbitfield mask);
extern void S3D_GLAPI glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
extern void S3D_GLAPI glClearDepth(GLdouble depth);
extern void S3D_GLAPI glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
extern void S3D_GLAPI glScissor(GLint x, GLint y, GLsizei w, GLsizei h);
extern void S3D_GLAPI glDepthFunc(GLenum func);
extern void S3D_GLAPI glDepthMask(GLboolean flag);
extern void S3D_GLAPI glDepthRange(GLdouble near_, GLdouble far_);
extern void S3D_GLAPI glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
extern void S3D_GLAPI glBlendFunc(GLenum sfactor, GLenum dfactor);
extern void S3D_GLAPI glCullFace(GLenum mode);
extern void S3D_GLAPI glFrontFace(GLenum mode);
extern void S3D_GLAPI glLineWidth(GLfloat width);
extern void S3D_GLAPI glPolygonMode(GLenum face, GLenum mode);
extern void S3D_GLAPI glPolygonOffset(GLfloat factor, GLfloat units);

extern const GLubyte* S3D_GLAPI glGetString(GLenum name);
extern void S3D_GLAPI glGetIntegerv(GLenum pname, GLint* params);
extern GLenum S3D_GLAPI glGetError(void);
extern void S3D_GLAPI glPixelStorei(GLenum pname, GLint param);
extern void S3D_GLAPI glFinish(void);
extern void S3D_GLAPI glFlush(void);

extern void S3D_GLAPI glGenTextures(GLsizei n, GLuint* textures);
extern void S3D_GLAPI glDeleteTextures(GLsizei n, const GLuint* textures);
extern void S3D_GLAPI glBindTexture(GLenum target, GLuint texture);
extern void S3D_GLAPI glTexImage2D(GLenum target, GLint level, GLint internalformat,
                                   GLsizei width, GLsizei height, GLint border,
                                   GLenum format, GLenum type, const void* pixels);
extern void S3D_GLAPI glTexSubImage2D(GLenum target, GLint level, GLint xoffset,
                                      GLint yoffset, GLsizei width, GLsizei height,
                                      GLenum format, GLenum type, const void* pixels);
extern void S3D_GLAPI glTexParameteri(GLenum target, GLenum pname, GLint param);
extern void S3D_GLAPI glTexParameterf(GLenum target, GLenum pname, GLfloat param);

extern void S3D_GLAPI glDrawArrays(GLenum mode, GLint first, GLsizei count);
extern void S3D_GLAPI glDrawElements(GLenum mode, GLsizei count, GLenum type,
                                     const void* indices);
extern void S3D_GLAPI glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format,
                                   GLenum type, void* pixels);

}  // extern "C"
