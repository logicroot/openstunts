#include "world/material.h"

namespace s3d::world {

namespace {

// material_color_list (dseg.asm 21106..21234): one VGA palette index per
// material. Copied verbatim from the reference.
constexpr uint8_t kMaterialColorList[kMaterialCount] = {
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
    108, 116,  15,  28,  29,  14,  28,  31,  14, 200, 198, 196, 112, 114, 116, 194,
    197, 200, 146,  37,  35, 181,  29,  31,  19,   3,  11,  27,   0,   4,   4,  12,
    156, 154, 152, 150,  42,  40,  38,  37,  27,  26,  25,  24,  72,  70,  68,  66,
    123, 121, 120, 117,  92,  90,  88,  87, 173, 171, 169, 167,  20,  19,  18,  17,
     77,  76,  74,  73,  45,  44,  42,  41, 159, 175, 174, 172,  29,  28,  18,  90,
     15,   7, 200, 219, 136,  99, 101, 103, 104, 106,  17,  20,  60,  77,  46,  61,
     45, 202, 190, 186, 183, 180,   0,  28,  30,  16,  20,  68,  54,  39,  43,  12,
     17,
};

// material_pattern_list (dseg.asm 21235..21363): non-zero selects a patterned
// fill. Duplicated from the reference for completeness.
constexpr uint8_t kMaterialPatternList[kMaterialCount] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   1,   1,   1,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   1,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   0,
      0,
};

}  // namespace

uint8_t material_color_index(uint8_t material) {
  return material < kMaterialCount ? kMaterialColorList[material] : 0;
}

bool material_patterned(uint8_t material) {
  return material < kMaterialCount && kMaterialPatternList[material] != 0;
}

glm::vec4 material_color(uint8_t material, const Palette& palette) {
  const uint8_t idx = material_color_index(material);
  return glm::vec4(static_cast<float>(palette.rgb8[idx][0]) / 255.0f,
                   static_cast<float>(palette.rgb8[idx][1]) / 255.0f,
                   static_cast<float>(palette.rgb8[idx][2]) / 255.0f, 1.0f);
}

}  // namespace s3d::world
