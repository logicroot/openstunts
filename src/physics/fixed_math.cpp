#include "physics/fixed_math.h"

// Tables copied verbatim from restunts (math.c `sintab`, sdl/main.cpp `atantable`).

namespace s3d::fixed {

static const int16_t kSinTab[257] = {
    0, 101, 201, 302, 402, 503, 603, 704, 804, 904, 1005, 1105, 1205, 1306, 1406, 1506,
    1606, 1706, 1806, 1906, 2006, 2105, 2205, 2305, 2404, 2503, 2603, 2702, 2801, 2900, 2999, 3098,
    3196, 3295, 3393, 3492, 3590, 3688, 3786, 3883, 3981, 4078, 4176, 4273, 4370, 4467, 4563, 4660,
    4756, 4852, 4948, 5044, 5139, 5235, 5330, 5425, 5520, 5614, 5708, 5803, 5897, 5990, 6084, 6177,
    6270, 6363, 6455, 6547, 6639, 6731, 6823, 6914, 7005, 7096, 7186, 7276, 7366, 7456, 7545, 7635,
    7723, 7812, 7900, 7988, 8076, 8163, 8250, 8337, 8423, 8509, 8595, 8680, 8765, 8850, 8935, 9019,
    9102, 9186, 9269, 9352, 9434, 9516, 9598, 9679, 9760, 9841, 9921, 10001, 10080, 10159, 10238, 10316,
    10394, 10471, 10549, 10625, 10702, 10778, 10853, 10928, 11003, 11077, 11151, 11224, 11297, 11370, 11442, 11514,
    11585, 11656, 11727, 11797, 11866, 11935, 12004, 12072, 12140, 12207, 12274, 12340, 12406, 12472, 12537, 12601,
    12665, 12729, 12792, 12854, 12916, 12978, 13039, 13100, 13160, 13219, 13279, 13337, 13395, 13453, 13510, 13567,
    13623, 13678, 13733, 13788, 13842, 13896, 13949, 14001, 14053, 14104, 14155, 14206, 14256, 14305, 14354, 14402,
    14449, 14497, 14543, 14589, 14635, 14680, 14724, 14768, 14811, 14854, 14896, 14937, 14978, 15019, 15059, 15098,
    15137, 15175, 15213, 15250, 15286, 15322, 15357, 15392, 15426, 15460, 15493, 15525, 15557, 15588, 15619, 15649,
    15679, 15707, 15736, 15763, 15791, 15817, 15843, 15868, 15893, 15917, 15941, 15964, 15986, 16008, 16029, 16049,
    16069, 16088, 16107, 16125, 16143, 16160, 16176, 16192, 16207, 16221, 16235, 16248, 16261, 16273, 16284, 16295,
    16305, 16315, 16324, 16332, 16340, 16347, 16353, 16359, 16364, 16369, 16373, 16376, 16379, 16381, 16383, 16384,
    16384,
};

static const uint8_t kAtanTab[258] = {
    0x00, 0x01, 0x01, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x06, 0x06, 0x07, 0x08, 0x08, 0x09, 0x0A,
    0x0A, 0x0B, 0x0B, 0x0C, 0x0D, 0x0D, 0x0E, 0x0F, 0x0F, 0x10, 0x10, 0x11, 0x12, 0x12, 0x13, 0x14,
    0x14, 0x15, 0x16, 0x16, 0x17, 0x17, 0x18, 0x19, 0x19, 0x1A, 0x1B, 0x1B, 0x1C, 0x1C, 0x1D, 0x1E,
    0x1E, 0x1F, 0x1F, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x24, 0x24, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x30, 0x30, 0x31,
    0x31, 0x32, 0x33, 0x33, 0x34, 0x34, 0x35, 0x35, 0x36, 0x37, 0x37, 0x38, 0x38, 0x39, 0x39, 0x3A,
    0x3A, 0x3B, 0x3C, 0x3C, 0x3D, 0x3D, 0x3E, 0x3E, 0x3F, 0x3F, 0x40, 0x41, 0x41, 0x42, 0x42, 0x43,
    0x43, 0x44, 0x44, 0x45, 0x45, 0x46, 0x46, 0x47, 0x47, 0x48, 0x48, 0x49, 0x4A, 0x4A, 0x4B, 0x4B,
    0x4C, 0x4C, 0x4D, 0x4D, 0x4E, 0x4E, 0x4F, 0x4F, 0x50, 0x50, 0x51, 0x51, 0x52, 0x52, 0x53, 0x53,
    0x54, 0x54, 0x54, 0x55, 0x55, 0x56, 0x56, 0x57, 0x57, 0x58, 0x58, 0x59, 0x59, 0x5A, 0x5A, 0x5B,
    0x5B, 0x5B, 0x5C, 0x5C, 0x5D, 0x5D, 0x5E, 0x5E, 0x5F, 0x5F, 0x60, 0x60, 0x60, 0x61, 0x61, 0x62,
    0x62, 0x63, 0x63, 0x63, 0x64, 0x64, 0x65, 0x65, 0x66, 0x66, 0x66, 0x67, 0x67, 0x68, 0x68, 0x68,
    0x69, 0x69, 0x6A, 0x6A, 0x6A, 0x6B, 0x6B, 0x6C, 0x6C, 0x6C, 0x6D, 0x6D, 0x6E, 0x6E, 0x6E, 0x6F,
    0x6F, 0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x72, 0x72, 0x73, 0x73, 0x73, 0x74, 0x74, 0x74, 0x75,
    0x75, 0x76, 0x76, 0x76, 0x77, 0x77, 0x77, 0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x7A, 0x7A, 0x7A,
    0x7B, 0x7B, 0x7B, 0x7C, 0x7C, 0x7C, 0x7D, 0x7D, 0x7D, 0x7E, 0x7E, 0x7E, 0x7F, 0x7F, 0x7F, 0x80,
    0x80, 0x00,
};

int16_t sin_fast(unsigned short s) {
  const unsigned char c = static_cast<unsigned char>(s & 0xFF);
  switch ((s >> 8) & 3) {
    case 0: return kSinTab[c];
    case 1: return kSinTab[0x100 - c];
    case 2: return static_cast<int16_t>(-kSinTab[c]);
    default: return static_cast<int16_t>(-kSinTab[0x100 - c]);
  }
}

int16_t cos_fast(unsigned short s) {
  return sin_fast(static_cast<unsigned short>(s + 0x100));
}

int polar_angle(int z, int y) {
  unsigned flag = 0;
  int temp, result = 0;
  uint32_t index;

  if (z < 0) {
    flag |= 4;
    z = -z;
  }
  if (y < 0) {
    flag |= 2;
    y = -y;
  }

  if (z == y) {
    if (z == 0) return 0;  // original returns an uninitialized value here
    result = 0x80;
  } else {
    if (z > y) {
      temp = z;
      z = y;
      y = temp;
      flag |= 1;
    }
    index = (static_cast<uint32_t>(z) << 16) / static_cast<uint32_t>(y);
    if ((index & 0xFF) >= 0x80) index += 0x100;  // round upwards
    result = kAtanTab[index >> 8];
  }

  switch (flag) {
    case 0: return result;
    case 1: return -result + 0x100;
    case 2: return -result + 0x200;
    case 3: return result + 0x100;
    case 4: return -result;
    case 5: return result - 0x100;
    case 6: return result - 0x200;
    default: return -(result + 0x100);
  }
}

int polar_radius2d(int z, int y) {
  int result = polar_angle(z, y);
  if (result < 0) result = -result;
  if (result >= 0x100) result = -(result - 0x200);

  if (result <= 0x80) {
    result = cos_fast(static_cast<unsigned short>(result));
    if (y < 0) y = -y;
    return static_cast<int>((static_cast<uint32_t>(y) << 14) / static_cast<uint32_t>(result));
  }
  result = sin_fast(static_cast<unsigned short>(result));
  if (z < 0) z = -z;
  return static_cast<int>((static_cast<uint32_t>(z) << 14) / static_cast<uint32_t>(result));
}

int polar_radius3d(const Vec3& v) {
  return polar_radius2d(polar_radius2d(v.x, v.y), v.z);
}

void mat_mul_vector(const Vec3& in, const Mat3& mat, Vec3* out) {
  int16_t ox = 0, oy = 0, oz = 0;

  if (mat.m[0] != 0 && in.x != 0) ox = static_cast<int16_t>((int32_t(mat.m[0]) * in.x) >> 14);
  if (mat.m[1] != 0 && in.y != 0) ox = wrap16(ox + static_cast<int16_t>((int32_t(mat.m[1]) * in.y) >> 14));
  if (mat.m[2] != 0 && in.z != 0) ox = wrap16(ox + static_cast<int16_t>((int32_t(mat.m[2]) * in.z) >> 14));

  if (mat.m[3] != 0 && in.x != 0) oy = static_cast<int16_t>((int32_t(mat.m[3]) * in.x) >> 14);
  if (mat.m[4] != 0 && in.y != 0) oy = wrap16(oy + static_cast<int16_t>((int32_t(mat.m[4]) * in.y) >> 14));
  if (mat.m[5] != 0 && in.z != 0) oy = wrap16(oy + static_cast<int16_t>((int32_t(mat.m[5]) * in.z) >> 14));

  if (mat.m[6] != 0 && in.x != 0) oz = static_cast<int16_t>((int32_t(mat.m[6]) * in.x) >> 14);
  if (mat.m[7] != 0 && in.y != 0) oz = wrap16(oz + static_cast<int16_t>((int32_t(mat.m[7]) * in.y) >> 14));
  if (mat.m[8] != 0 && in.z != 0) oz = wrap16(oz + static_cast<int16_t>((int32_t(mat.m[8]) * in.z) >> 14));

  out->x = ox;
  out->y = oy;
  out->z = oz;
}

void mat_multiply(const Mat3& rmat, const Mat3& lmat, Mat3* out) {
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      int16_t acc = 0;
      for (int k = 0; k < 3; ++k) {
        const int16_t r = rmat.at(i, k);
        const int16_t l = lmat.at(k, j);
        if (r != 0 && l != 0) {
          acc = wrap16(acc + static_cast<int16_t>((int32_t(r) * l) >> 14));
        }
      }
      out->at(i, j) = acc;
    }
  }
}

void mat_invert(const Mat3& in, Mat3* out) {
  out->m[0] = in.m[0];
  out->m[1] = in.m[3];
  out->m[2] = in.m[6];
  out->m[3] = in.m[1];
  out->m[4] = in.m[4];
  out->m[5] = in.m[7];
  out->m[6] = in.m[2];
  out->m[7] = in.m[5];
  out->m[8] = in.m[8];
}

void mat_rot_x(int angle, Mat3* out) {
  const int c = cos_fast(static_cast<unsigned short>(angle));
  const int s = sin_fast(static_cast<unsigned short>(angle));
  out->m[0] = 0x4000; out->m[1] = 0;           out->m[2] = 0;
  out->m[3] = 0;      out->m[4] = c;           out->m[5] = -s;
  out->m[6] = 0;      out->m[7] = s;           out->m[8] = c;
}

void mat_rot_y(int angle, Mat3* out) {
  const int c = cos_fast(static_cast<unsigned short>(angle));
  const int s = sin_fast(static_cast<unsigned short>(angle));
  out->m[0] = c;      out->m[1] = 0;           out->m[2] = s;
  out->m[3] = 0;      out->m[4] = 0x4000;      out->m[5] = 0;
  out->m[6] = -s;     out->m[7] = 0;           out->m[8] = c;
}

void mat_rot_z(int angle, Mat3* out) {
  const int c = cos_fast(static_cast<unsigned short>(angle));
  const int s = sin_fast(static_cast<unsigned short>(angle));
  out->m[0] = c;      out->m[1] = -s;          out->m[2] = 0;
  out->m[3] = s;      out->m[4] = c;           out->m[5] = 0;
  out->m[6] = 0;      out->m[7] = 0;           out->m[8] = 0x4000;
}

Mat3 mat_rot_zxy(int z, int x, int y, int unk) {
  Mat3 mz, mx, my, tmp;
  mat_rot_z(z, &mz);
  mat_rot_x(x, &mx);
  mat_rot_y(y, &my);

  // The original's MATRIX stores its columns contiguously (math.h `vals[]` is
  // _11,_21,_31,_12,...), so its mat_multiply(A, B) yields B*A in row-major
  // terms. Composing through our row-major multiply therefore swaps the
  // operands: mat_rot_zxy(z, x, y) must produce my*mx*mz (apply z, then x,
  // then y), which is what the original returns.
  Mat3 result;
  if ((unk & 1) != 0) {
    mat_multiply(mx, my, &tmp);
    mat_multiply(mz, tmp, &result);
    return result;
  }
  mat_multiply(mx, mz, &tmp);
  mat_multiply(my, tmp, &result);
  return result;
}

int16_t multiply_and_scale(int16_t a, int16_t b) {
  const int32_t mul = static_cast<int32_t>((int64_t)a * b * 4);
  return static_cast<int16_t>((mul >> 16) + ((mul & 0x8000) >> 15));
}

int vec_normal_inner_product(int x, int y, int z, const Vec3& normal) {
  return static_cast<int>((int32_t(normal.x) * x + int32_t(normal.z) * z +
                           int32_t(normal.y) * y) /
                          0x2000);
}

}  // namespace s3d::fixed
