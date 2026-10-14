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

// car1 (the full-detail racing body) keeps its four wheels in vertices 8..31,
// six vertices per wheel in the order front-left, front-right, rear-right,
// rear-left. The original animates exactly this block every frame in
// `sub_204AE` (seg004.asm 3830 / c/shape3d.c 5012), called from frame.c with
// the car's steering angle and `car_rc2` (the per-wheel suspension travel).
inline constexpr int kCarWheelFirstVertex = 8;
inline constexpr int kCarWheelVertexCount = 24;

// Visual wheel pose fed to `apply_car_wheel_pose`.
//
// `posed` mirrors the original's steering-angle cache (`word_443E8[4]`,
// seg004.asm 3855): `sub_204AE` re-applies the front-wheel x/z transform only
// when the steering angle differs from the cached one, and the cache starts at
// 0. So the authored mesh is shown while the car has never steered, and after
// the first steering input the transform is re-applied on every change --
// including back at angle 0, where the formula degenerates to a point
// reflection about each front-wheel pivot.
struct CarWheelPose {
  bool posed = false;          // has the front-wheel transform ever been applied?
  int16_t steering_angle = 0;  // CARSTATE.car_steeringAngle, 1/1024 turn units
  int16_t suspension[4] = {0, 0, 0, 0};  // CARSTATE.car_rc2, 1/64 world units

  bool operator==(const CarWheelPose& o) const {
    return posed == o.posed && steering_angle == o.steering_angle &&
           suspension[0] == o.suspension[0] && suspension[1] == o.suspension[1] &&
           suspension[2] == o.suspension[2] && suspension[3] == o.suspension[3];
  }
  bool operator!=(const CarWheelPose& o) const { return !(*this == o); }
};

// Applies the pose to a car1 shape in place, exactly mirroring `sub_204AE`:
//   * when `pose.posed` is set, the two front wheels are transformed about
//     their own pivots by half the steering angle (the original halves the
//     angle with a 16-bit arithmetic shift);
//   * every wheel's vertices drop by (|car_rc2[wheel]| >> 6) world units.
// A shape without the 24-vertex wheel block (i.e. car2) is left unchanged.
// Exposed for tests.
void apply_car_wheel_pose(Shape3D* shape, const CarWheelPose& pose);

// Deliberate deviation from the original data (see Config::wheel_scale). The
// authored wheels are 6x6x8-unit corner blocks -- faithful, but at modern
// resolutions they read as "missing tyres". This scales each of the four
// wheels about its own centre by `scale` (1.0 leaves them alone), so the tyres
// become clearly visible without modifying the on-disk assets. A shape without
// the wheel block (car2) is left unchanged.
void scale_car_wheels(Shape3D* shape, float scale);

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
//
// When `wheel_pose` is non-null the car1 wheel block is animated with it first
// (frame.c sub_204AE: steering + suspension). car2 has no wheels and ignores it.
// `wheel_scale` then enlarges the posed wheels about their own centres (see
// scale_car_wheels); 1.0 keeps the authored size.
void append_car_model(MeshBuilder& mb, const ShapeLibrary& car_lib, const glm::mat4& model,
                      const PaintColorFn& color_for, int material = 0,
                      const CarWheelPose* wheel_pose = nullptr, float wheel_scale = 1.0f);

}  // namespace s3d::world
