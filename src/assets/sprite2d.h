#pragma once
// 2D sprites from the Stunts .PVS 2D-shape archives (e.g. SDGAME2.PVS).
//
// A .PVS decompresses to a resource container (assets/res_file.h) whose chunks
// are SHAPE2D records:
//
//   struct SHAPE2D { u16 width, u16 height, u16 unk1, u16 unk2,
//                    u16 pos_x, u16 pos_y, u8 unk3, u8 unk4, u8 unk5, u8 unk6; };
//   u8 pixels[width * height];   // palette indices, 0xFF = transparent
//
// The in-game direction arrows come from SDGAME2.PVS ("left" / "righ", see
// seg003.asm load_sdgame2_shapes -> locate_many_resources("ex01ex02ex03leftrigh")).
//
// Reference: reference/restunts/src/restunts/c/shape2d.c and seg012.asm
// (sprite_putimage_transparent, whose `incnums` colour key makes 0xFF
// transparent).

#include <cstdint>
#include <string>
#include <vector>

namespace s3d {

struct Sprite2D {
  int width = 0;
  int height = 0;
  // SHAPE2D.s2d_unk1/s2d_unk2. Used as the pixel-space anchor for the scaled
  // blit (shape_op_explosion places this pixel at the caller's centre), so they
  // are the sprite's pivot in authored pixels.
  int pivot_x = 0;
  int pivot_y = 0;
  int pos_x = 0;  // SHAPE2D.s2d_pos_x (top-left, the no-arg sprite_putimage anchor)
  int pos_y = 0;  // SHAPE2D.s2d_pos_y
  std::vector<uint8_t> pixels;  // width * height palette indices (0xFF clear)

  bool valid() const {
    return width > 0 && height > 0 &&
           pixels.size() == static_cast<size_t>(width) * static_cast<size_t>(height);
  }
};

// Loads the named sprite from a .PVS archive. Returns false when the archive or
// the sprite is missing, or the record is malformed.
bool load_sprite2d(const std::string& pvs_path, const std::string& id,
                   Sprite2D* out);

}  // namespace s3d
