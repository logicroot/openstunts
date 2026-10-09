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

// Appends the racing car positioned/oriented by `model` using the full-detail
// body (`car1`) when present, else the low-detail body (`car2`). Wheels are
// drawn in a dark tread colour; everything else uses `body_color`.
void append_car_model(MeshBuilder& mb, const ShapeLibrary& car_lib, const glm::mat4& model,
                      const glm::vec4& body_color);

}  // namespace s3d::world
