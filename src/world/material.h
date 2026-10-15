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

// load_skybox (seg003.asm loc_1D88E) reads three fixed colours straight out of
// material_color_list, indexed by byte offset (each entry is a word, so the
// element index is offset / 2):
//   sky    = element 0x11 (offset 0x22)
//   ground = element 0x10 (offset 0x20)
//   water  = element 0x64 (offset 0xC8)
// Water is the fill colour of the sinking crash overlay (do_sinking).
inline constexpr uint8_t kMaterialSkyboxSky = 0x11;
inline constexpr uint8_t kMaterialSkyboxGround = 0x10;
inline constexpr uint8_t kMaterialSkyboxWater = 0x64;

// VGA palette index for `material` (0..kMaterialCount-1). Out-of-range values
// fall back to 0 (black), matching a zero-initialised table read.
uint8_t material_color_index(uint8_t material);

// True when the material is drawn with a fill pattern rather than a flat fill
// (material_pattern_list entry != 0). Flat colours are still correct for the
// patterned materials, so callers may ignore this.
bool material_patterned(uint8_t material);

// The screen-space dither mask for `material` (material_pattern2_list). The
// original fills a patterned polygon with `draw_patterned_lines` (seg012.asm),
// which uses this 16-bit mask as an 8-wide tile: on each scanline it draws the
// material colour where a bit is set and leaves the pixel behind it where a bit
// is clear, alternating between the low and high byte from one scanline to the
// next. So a patterned material is stippled over whatever was drawn under it.
//
// Returns 0 for flat materials and for any material whose mask is 0 (material
// 118, the windmill's transparent animation mask, which therefore draws
// nothing).
uint16_t material_pattern(uint8_t material);

// True when the material draws nothing. Material 118 is the "windmill
// animation mask" — the original renders it as a transparent stipple used to
// hide the sail positions that are not currently lit; there is no palette
// entry for it. Polygons using it must be skipped.
bool material_transparent(uint8_t material);

// Flat opaque RGBA for `material` sampled from `palette`.
glm::vec4 material_color(uint8_t material, const Palette& palette);

}  // namespace s3d::world
