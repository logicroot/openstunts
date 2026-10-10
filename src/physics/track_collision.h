#pragma once
// Track collision model (planes, walls, element-based collision) ported from
// restunts `seg004.asm::build_track_object` and `math.c`
// (`plane_origin_op` / `plane_rotate_op`).
//
// The original engine stores the plane (`plan`) and wall (`wall`) tables in
// GAME.PRE and the per-tile element/terrain maps in the loaded track. This
// module joins the two: `build_track_object()` picks the active plane/wall for
// a world point, and `plane_signed_distance()`/`transform_wheel_travel_to_world`
// reproduce the two math.c helpers.

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "assets/trk.h"
#include "physics/fixed_math.h"
#include "world/track_objects.h"

namespace s3d::physics {

struct Plane {
  int16_t plane_yz;   // angleYZ (1/1024 turn)
  int16_t plane_xy;   // angleXY (1/1024 turn)
  fixed::Vec3 plane_origin;
  fixed::Vec3 plane_normal;   // length 0x2000
  fixed::Mat3 plane_rotation; // 3x3, 1.14 fixed point
};

struct TrackWall {
  int16_t orientation;  // angleXZ
  int16_t x;
  int16_t z;
};

// Full result of one `build_track_object` evaluation. Only the fields the
// reduced physics needs are materialized; the rest is kept for tests/debugging.
struct CollisionSample {
  int16_t plane_index = 0;      // final PLANE index (0 = flat ground)
  int16_t wall_index = -1;      // -1 = none
  int16_t terrain_height = 0;   // accumulated terrainHeight
  int8_t surface_type = 4;      // CAR_SURFACE_* (grass default)
  int8_t wall_collision_enabled = 1;
  int16_t elem_x_center = 0;
  int16_t elem_z_center = 0;
  int16_t wall_start_x = 0;
  int16_t wall_start_z = 0;
  int16_t wall_orientation = 0;
  int16_t wall_height = -12;    // 0xFFF4 default
  int16_t wall_lower_bound = -1000;
  int16_t physical_model = -1;
};

// CAR_SURFACE_* ids, matching car_state.h / track_elevation.h.
enum class CarSurfaceType : int8_t {
  Paved = 1,
  Dirt = 2,
  Ice = 3,
  Grass = 4,
  Water = 5,
};

class TrackCollision {
 public:
  TrackCollision();
  explicit TrackCollision(const std::string& game_pre_path);

  // Loads the `plan`/`wall` tables from GAME.PRE (decompressed).
  bool load_from_game_pre(const std::string& game_pre_path);
  bool is_loaded() const { return loaded_; }

  // The element/terrain grid used to resolve physical models. Must be set
  // before build_track_object() is called.
  void set_track(const s3d::Track* track) { track_ = track; }
  const s3d::Track* track() const { return track_; }

  // Ported from seg004.asm `build_track_object`: resolves the physical model
  // covering `world_pos` and fills `out`. `next_world_pos` is the position
  // after this tick's wheel travel (used for wall tests).
  void build_track_object(const fixed::Vec3& world_pos, const fixed::Vec3& next_world_pos,
                          CollisionSample* out) const;
  void build_track_object_from_track(const s3d::Track& track, const fixed::Vec3& world_pos,
                                     const fixed::Vec3& next_world_pos,
                                     CollisionSample* out) const;

  // Ported from math.c `plane_origin_op`: signed distance of (x,y,z) from
  // `plane_index`, including the accumulated `terrain_height` and (for
  // plane_index >= 4) the element centre offset.
  int plane_signed_distance(int plane_index, int x, int y, int z, int terrain_height,
                            int elem_x_center, int elem_z_center) const;

  // Ported from math.c `plane_rotate_op`: rotates a wheel travel vector
  // (0, 0, travel) into world space, honoring the active plane (or the no-plane
  // fallback when `plane_index < 0`). `car_yaw/pitch/roll` are 1/1024-turn.
  void transform_wheel_travel_to_world(int16_t travel, int16_t wheel_heading,
                                       const fixed::Mat3& car_to_world, int16_t car_yaw,
                                       int16_t car_pitch, int16_t car_roll, int plane_index,
                                       fixed::Vec3* wheel_world_travel) const;

  const Plane* planes() const { return planes_.data(); }
  size_t plane_count() const { return planes_.size(); }
  const TrackWall* walls() const { return walls_.data(); }
  size_t wall_count() const { return walls_.size(); }

 private:
  bool loaded_ = false;
  const s3d::Track* track_ = nullptr;
  std::vector<Plane> planes_;
  std::vector<TrackWall> walls_;
};

}  // namespace s3d::physics
