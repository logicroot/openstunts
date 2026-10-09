#pragma once
// Minimal logging facility. Kept dependency-free on purpose.

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace s3d {

enum class LogLevel { Debug, Info, Warn, Error };

inline void log_line(LogLevel level, const char* fmt, ...) {
  const char* tag = "INFO";
  switch (level) {
    case LogLevel::Debug: tag = "DEBUG"; break;
    case LogLevel::Info: tag = "INFO"; break;
    case LogLevel::Warn: tag = "WARN"; break;
    case LogLevel::Error: tag = "ERROR"; break;
  }
  std::fprintf(stderr, "[%s] ", tag);
  va_list args;
  va_start(args, fmt);
  std::vfprintf(stderr, fmt, args);
  va_end(args);
  std::fputc('\n', stderr);
}

}  // namespace s3d

#define S3D_DEBUG(...) ::s3d::log_line(::s3d::LogLevel::Debug, __VA_ARGS__)
#define S3D_INFO(...) ::s3d::log_line(::s3d::LogLevel::Info, __VA_ARGS__)
#define S3D_WARN(...) ::s3d::log_line(::s3d::LogLevel::Warn, __VA_ARGS__)
#define S3D_ERROR(...) ::s3d::log_line(::s3d::LogLevel::Error, __VA_ARGS__)

#define S3D_CHECK(cond, ...)                                       \
  do {                                                             \
    if (!(cond)) {                                                 \
      ::s3d::log_line(::s3d::LogLevel::Error, __VA_ARGS__);        \
      std::abort();                                                \
    }                                                              \
  } while (0)
