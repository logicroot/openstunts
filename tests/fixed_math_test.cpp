#include "physics/fixed_math.h"
#include "tiny_test.h"

using namespace s3d::fixed;

TEST(fixed_sin_cos_table_anchors) {
  EXPECT_EQ(sin_fast(0), 0);
  EXPECT_EQ(sin_fast(0x100), 16384);  // 90 degrees
  EXPECT_EQ(sin_fast(0x80), 11585);   // 45 degrees
  EXPECT_EQ(cos_fast(0), 16384);
  EXPECT_EQ(sin_fast(0x200), 0);      // 180 degrees
  EXPECT_EQ(sin_fast(0x300), -16384); // 270 degrees
}

TEST(fixed_polar_angle_quadrants) {
  EXPECT_EQ(polar_angle(0, 5), 0x000);
  EXPECT_EQ(polar_angle(5, 0), 0x100);
  EXPECT_EQ(polar_angle(5, 5), 0x080);
  EXPECT_EQ(polar_angle(0, -5), 0x200);
  EXPECT_EQ(polar_angle(-5, 0), -0x100);  // original returns signed angles
  EXPECT_EQ(polar_angle(-5, -5), -0x180);
}

TEST(fixed_polar_radius) {
  EXPECT_EQ(polar_radius2d(0, 0x4000), 0x4000);
  EXPECT_EQ(polar_radius2d(0x4000, 0), 0x4000);
}

TEST(fixed_multiply_and_scale_roundtrip) {
  EXPECT_EQ(multiply_and_scale(0x4000, 0x4000), 0x4000);
  EXPECT_EQ(multiply_and_scale(0x4000, 0), 0);
  EXPECT_EQ(multiply_and_scale(0x2000, 0x4000), 0x2000);
}

TEST(fixed_matrix_identity_and_rotation) {
  Mat3 id;
  Vec3 in{100, -200, 300};
  Vec3 out;
  mat_mul_vector(in, id, &out);
  EXPECT_EQ(out.x, 100);
  EXPECT_EQ(out.y, -200);
  EXPECT_EQ(out.z, 300);

  // Rotating (0x4000, 0, 0) by 90 degrees about Z gives (0, 0x4000, 0).
  Mat3 rz;
  mat_rot_z(0x100, &rz);
  Vec3 unit{0x4000, 0, 0};
  Vec3 rot;
  mat_mul_vector(unit, rz, &rot);
  EXPECT_EQ(rot.x, 0);
  EXPECT_EQ(rot.y, 0x4000);
}
