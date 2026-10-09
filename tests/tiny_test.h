#pragma once
// Tiny dependency-free test harness. Tests register themselves and return the
// number of failures via s3d_test::run_all().

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace s3d_test {

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> r;
  return r;
}

inline int& failure_count() {
  static int c = 0;
  return c;
}

struct Registrar {
  Registrar(const std::string& name, std::function<void()> fn) {
    registry().push_back({name, std::move(fn)});
  }
};

inline int run_all() {
  int failed = 0;
  for (auto& t : registry()) {
    const int before = failure_count();
    t.fn();
    if (failure_count() != before) {
      std::printf("[FAIL] %s\n", t.name.c_str());
      ++failed;
    } else {
      std::printf("[ ok ] %s\n", t.name.c_str());
    }
  }
  std::printf("%zu tests, %d failed\n", registry().size(), failed);
  return failed == 0 ? 0 : 1;
}

}  // namespace s3d_test

#define TEST(name)                                                       \
  static void s3d_test_##name();                                         \
  static ::s3d_test::Registrar s3d_test_reg_##name(#name, s3d_test_##name); \
  static void s3d_test_##name()

#define EXPECT_TRUE(expr)                                                  \
  do {                                                                     \
    if (!(expr)) {                                                         \
      std::printf("  CHECK FAILED: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
      ++::s3d_test::failure_count();                                       \
    }                                                                      \
  } while (0)

#define EXPECT_EQ(a, b)                                                      \
  do {                                                                       \
    if (!((a) == (b))) {                                                     \
      std::printf("  CHECK FAILED: %s == %s (%s:%d)\n", #a, #b, __FILE__,    \
                  __LINE__);                                                 \
      ++::s3d_test::failure_count();                                         \
    }                                                                        \
  } while (0)
