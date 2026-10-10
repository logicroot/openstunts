#pragma once
// Material (paint) table ported from restunts `dseg.asm`
// (`material_color_list` / `material_pattern_list`).
//
// A P3S polygon carries one paint byte per material. That byte indexes the
// material table, which maps to a VGA palette entry used as a flat fill. The
// palette itself comes from the `!pal` resource (see assets/palette.h); the
// original stores these tables in the executable, so they are embedded here.
//
// Reference: restunts-bb11 `shape3d.c` (get_a_poly_info:
//   materialcolor = material_clrlist_ptr_cpy[materialtype];
//   materialpattern = material_patlist_ptr_cpy[materialtype];)
// and `asmorig/dseg.asm` material_color_list / material_pattern_list.

#include <cstdint>

#include <glm/glm.hpp>

#include "assets/palette.h"

namespace s3d::world {

// Number of entries in material_color_list.
inline constexpr int kMaterialCount = 129;

// VGA palette index for `material` (0..kMaterialCount-1). Out-of-range values
// fall back to 0 (black), matching a zero-initialised table read.
uint8_t material_color_index(uint8_t material);

// True when the material is drawn with a fill pattern rather than a flat fill
// (material_pattern_list entry != 0). Flat colours are still correct for the
// patterned materials, so callers may ignore this.
bool material_patterned(uint8_t material);

// Flat opaque RGBA for `material` sampled from `palette`.
glm::vec4 material_color(uint8_t material, const Palette& palette);

}  // namespace s3d::world
