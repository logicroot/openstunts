#include "physics/fixed_math.h"
#include "tiny_test.h"

using namespace s3d::fixed;

// Reference matrix math copied from restunts src/restunts/c/math.c
// (mat_rot_x/y/z, mat_multiply, mat_mul_vector, mat_rot_zxy). The original's
// MATRIX stores its members column-major (`vals` = _11,_21,_31,_12,...), while
// our Mat3 is row-major, so this harness keeps the original layout verbatim and
// lets us cross-check the ported helpers without shipping assets.
namespace refmat {
struct MATRIX { int vals[9]; };
struct VECTOR { int x, y, z; };
void rot_x(MATRIX* o, int a) {
  const int c = cos_fast(static_cast<unsigned short>(a));
  const int s = sin_fast(static_cast<unsigned short>(a));
  o->vals[0] = 0x4000; o->vals[1] = 0;      o->vals[2] = 0;
  o->vals[3] = 0;      o->vals[4] = c;      o->vals[5] = s;
  o->vals[6] = 0;      o->vals[7] = -s;     o->vals[8] = c;
}
void rot_y(MATRIX* o, int a) {
  const int c = cos_fast(static_cast<unsigned short>(a));
  const int s = sin_fast(static_cast<unsigned short>(a));
  o->vals[0] = c;      o->vals[1] = 0;      o->vals[2] = -s;
  o->vals[3] = 0;      o->vals[4] = 0x4000; o->vals[5] = 0;
  o->vals[6] = s;      o->vals[7] = 0;      o->vals[8] = c;
}
void rot_z(MATRIX* o, int a) {
  const int c = cos_fast(static_cast<unsigned short>(a));
  const int s = sin_fast(static_cast<unsigned short>(a));
  o->vals[0] = c;      o->vals[1] = s;      o->vals[2] = 0;
  o->vals[3] = -s;     o->vals[4] = c;      o->vals[5] = 0;
  o->vals[6] = 0;      o->vals[7] = 0;      o->vals[8] = 0x4000;
}
void multiply(const MATRIX* r, const MATRIX* l, MATRIX* out) {
  const int* rv = r->vals;
  const int* lv = l->vals;
  int* ov = out->vals;
  int counter = 9;
  while (counter > 0) {
    ov[0] = (rv[0] && lv[0]) ? (int)(((long long)rv[0] * lv[0]) >> 14) : 0;
    if (rv[1] && lv[3]) ov[0] += (int)(((long long)rv[1] * lv[3]) >> 14);
    if (rv[2] && lv[6]) ov[0] += (int)(((long long)rv[2] * lv[6]) >> 14);
    ov++;
    if (counter != 7 && counter != 4) {
      lv++;
    } else {
      lv -= 2;
      rv += 3;
    }
    counter--;
  }
}
void mul_vector(const VECTOR* in, const MATRIX* m, VECTOR* out) {
  const int* v = m->vals;
  out->x = (v[0] && in->x) ? (int)(((long long)v[0] * in->x) >> 14) : 0;
  if (v[3] && in->y) out->x += (int)(((long long)v[3] * in->y) >> 14);
  if (v[6] && in->z) out->x += (int)(((long long)v[6] * in->z) >> 14);
  out->y = (v[1] && in->x) ? (int)(((long long)v[1] * in->x) >> 14) : 0;
  if (v[4] && in->y) out->y += (int)(((long long)v[4] * in->y) >> 14);
  if (v[7] && in->z) out->y += (int)(((long long)v[7] * in->z) >> 14);
  out->z = (v[2] && in->x) ? (int)(((long long)v[2] * in->x) >> 14) : 0;
  if (v[5] && in->y) out->z += (int)(((long long)v[5] * in->y) >> 14);
  if (v[8] && in->z) out->z += (int)(((long long)v[8] * in->z) >> 14);
}
MATRIX zxy(int z, int x, int y, int unk) {
  MATRIX mz, mx, my, tmp, res;
  rot_z(&mz, z);
  rot_x(&mx, x);
  rot_y(&my, y);
  if ((unk & 1) != 0) {
    multiply(&my, &mx, &tmp);
    multiply(&tmp, &mz, &res);
  } else {
    multiply(&mz, &mx, &tmp);
    multiply(&tmp, &my, &res);
  }
  return res;
}
}  // namespace refmat

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

// The pitch (X) rotation is easy to get backwards, so pin down the exact
// handedness of seg024.asm:84-90 (mat_rot_x): for a positive angle the +z axis
// tips *downwards*, i.e. (0,0,1) -> (0, -sin, cos). The car pose feeds a
// negated pitch to this helper, so a nose-up car still ends up nose-up; the
// wrong sign made the wheels sink into ramps.
TEST(fixed_rot_x_matches_reference_handedness) {
  Mat3 mx;
  mat_rot_x(46, &mx);
  Vec3 forward{0, 0, 0x4000};
  Vec3 out;
  mat_mul_vector(forward, mx, &out);
  EXPECT_EQ(out.x, 0);
  EXPECT_EQ(out.y, -sin_fast(46));
  EXPECT_EQ(out.z, cos_fast(46));
  EXPECT_TRUE(out.y < 0 && out.z > 0);
}

// Cross-check every composed rotation against the reference implementation for
// both the `unk` branches used in-game (drive/camera pose uses 0, race-state
// interpolation uses 1).
TEST(fixed_rot_zxy_matches_reference) {
  const int values[] = {-900, -768, -512, -300, -256, -67, -46, 0, 46, 67, 100,
                        256,   300,  512,  700,  768,  900, 1000};
  const int inputs[3][3] = {{1234, -567, 1984}, {0, 0, 1984}, {-3000, 250, -77}};
  for (int unk = 0; unk <= 1; ++unk) {
    for (int zi = 0; zi < 18; ++zi) {
      for (int xi = 0; xi < 18; xi += 3) {
        for (int yi = 0; yi < 18; yi += 3) {
          const int z = values[zi];
          const int x = values[xi];
          const int y = values[yi];
          const refmat::MATRIX rm = refmat::zxy(z, x, y, unk);
          const Mat3 fm = mat_rot_zxy(z, x, y, unk);
          for (const auto& in : inputs) {
            refmat::VECTOR rin{in[0], in[1], in[2]};
            refmat::VECTOR rout{};
            refmat::mul_vector(&rin, &rm, &rout);

            Vec3 fin{static_cast<int16_t>(in[0]), static_cast<int16_t>(in[1]),
                     static_cast<int16_t>(in[2])};
            Vec3 fout;
            mat_mul_vector(fin, fm, &fout);

            EXPECT_EQ(fout.x, rout.x);
            EXPECT_EQ(fout.y, rout.y);
            EXPECT_EQ(fout.z, rout.z);
          }
        }
      }
    }
  }
}
