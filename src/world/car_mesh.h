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

#include <string>

#include "assets/shape3d.h"
#include "world/mesh_builder.h"

namespace s3d::world {

// Loads `ST<car_id>.P3S` from `asset_dir`. `car_id` is four characters, e.g.
// "COUN". Returns false if the archive cannot be read.
bool load_car_archive(const std::string& asset_dir, const std::string& car_id,
                      ShapeLibrary* out);

// Maps one car primitive's paint job to its fill colour:
//   paint 38    tyres/chassis, drawn in a dark tread colour;
//   paint 0x2D  rear lights, drawn in `backlight_color`;
//   else        livery bodywork, drawn in `body_color`.
//
// The rear-light paint job (0x2D) is the sentinel the original swaps for
// `backlights_paint_override` (shape3d seg006.asm loc_25D3C); frame.c sets that
// override to material 0x2E when coasting and 0x2F while braking, so the caller
// passes the corresponding material colour through `backlight_color`.
glm::vec4 car_primitive_color(uint8_t paint, const glm::vec4& body_color,
                              const glm::vec4& backlight_color);

// Appends the racing car positioned/oriented by `model` using the full-detail
// body (`car1`) when present, else the low-detail body (`car2`). Wheels are
// drawn in a dark tread colour; everything else uses `body_color`.
// `backlight_color` is the fill for the rear brake-light polygons; pass the
// material colour for the original `backlights_paint_override` value (0x2E when
// coasting, 0x2F while braking) so the lights brighten under braking.
void append_car_model(MeshBuilder& mb, const ShapeLibrary& car_lib, const glm::mat4& model,
                      const glm::vec4& body_color,
                      const glm::vec4& backlight_color = glm::vec4(0.67f, 0.0f, 0.0f, 1.0f));

}  // namespace s3d::world
