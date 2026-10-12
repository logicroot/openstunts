#pragma once
// Racing-car geometry.
//
// Each car ships as `ST<ID>.P3S` (e.g. `STCOUN.P3S`) containing three shapes:
//   car0  high-detail showroom body, authored at 20x the racing scale;
//   car1  full-detail racing body including the four wheels;
//   car2  low-detail racing body (no wheels).
// Physics uses the racing scale, so the in-game car is `car1` (or `car2`).
//
// Reference: restunts `shape3d_car.c` (shape3d_load_car_shapes) and
// `frame.c` (frame_add_car uses trkObjectList[2/3] shape 126/128).

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

#include "assets/palette.h"
#include "assets/shape3d.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"

namespace s3d::world {

// Paint job 0x2D marks the rear light polygons in every car body. The original
// never draws that livery: shape3d substitutes `backlights_paint_override`
// (seg006.asm loc_25D3C / c/shape3d.c loc_25D3C).
inline constexpr uint8_t kCarBacklightPaint = 0x2D;

// Loads `ST<car_id>.P3S` from `asset_dir`. `car_id` is four characters, e.g.
// "COUN". Returns false if the archive cannot be read.
bool load_car_archive(const std::string& asset_dir, const std::string& car_id,
                      ShapeLibrary* out);

// Maps one car primitive's paint job to its fill colour through the original
// material table (`material_color_list`), the same way the track and scenery
// shapes are shaded:
//   paint 0x2D  the rear-light sentinel (kCarBacklightPaint): the original
//               swaps it for `backlights_paint_override`, which `frame.c` sets
//               to material 0x2E when coasting and 0x2F while braking, so the
//               caller passes that colour through `backlight_color`;
//   else        the paint byte is a material index, resolved with material_color.
glm::vec4 car_primitive_color(uint8_t paint, const Palette& palette,
                              const glm::vec4& backlight_color);

// Appends the racing car positioned/oriented by `model` using the full-detail
// body (`car1`) when present, else the low-detail body (`car2`). `color_for`
// maps each primitive's paint byte to a fill colour (use car_primitive_color
// to resolve it through the material table).
//
// `material` selects the livery/paint-job slot exactly like the original
// TRANSFORMEDSHAPE3D.ts_material field: `frame.c` sets it to
// `gameconfig.game_playermaterial` (default 0) for the player car and
// `gameconfig.game_opponentmaterial` for the opponent, picking one of the
// shape's `num_paints` livery variants (0..6 for the retail cars). An
// out-of-range slot clamps to 0.
void append_car_model(MeshBuilder& mb, const ShapeLibrary& car_lib, const glm::mat4& model,
                      const PaintColorFn& color_for, int material = 0);

}  // namespace s3d::world
