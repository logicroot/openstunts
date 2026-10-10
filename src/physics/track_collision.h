#pragma once
// Track collision model (planes, walls, element-based collision) ported from restunts.

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
  int16_t plane_yz;
  int16_t plane_xy;
  fixed::Vec3 plane_origin;
  fixed::Vec3 plane_normal;
  fixed::Mat3 plane_rotation;
};

struct TrackWall {
  int16_t orientation;
  int16_t x;
  int16_t z;
};

struct TrackCollisionSnapshot {
  int16_t plane_index;
  int16_t wall_index;
  int16_t wall_height;
  int16_t wall_lower_bound;
  uint8_t corkscrew;
  int8_t surface_type;
  int8_t wall_collision_enabled;
  int16_t terrain_height;
  int16_t element_x;
  int16_t element_z;
  int16_t wall_x;
  int16_t wall_z;
  int16_t wall_orientation;
};

enum class CarSurfaceType : int8_t {
  Paved = 1,
  Dirt = 2,
  Ice = 3,
  Grass = 4,
  Water = 5,
};

struct TrackObjectSample {
  fixed::Vec3 position;
  fixed::Vec3 next_position;
  int16_t height;
  int16_t next_height;
  int16_t physical_model;
  int16_t surface_type;
  int16_t absolute_x;
  int16_t absolute_z;
  int16_t wall_orientation_modifier;
};

struct CollisionSample {
  int16_t plane_index = -1;  // -1 = none
  int16_t wall_index = -1;
  int16_t terrain_height = 0;
  int8_t surface_type = static_cast<int8_t>(CarSurfaceType::Grass);
  int8_t wall_collision_enabled = 1;
  int16_t elem_x_center = 0;
  int16_t elem_z_center = 0;
  int16_t wall_start_x = 0;
  int16_t wall_start_z = 0;
  int16_t wall_orientation = 0;
  int16_t wall_height = 384;  // default
  int16_t wall_lower_bound = -1000;
};

class TrackCollision {
 public:
  TrackCollision();
  explicit TrackCollision(const std::string& game_pre_path);

  bool load_from_game_pre(const std::string& game_pre_path);
  bool is_loaded() const { return loaded_; }

  void build_track_object(const fixed::Vec3& world_pos, const fixed::Vec3& next_world_pos, CollisionSample* out);
  void build_track_object_from_track(const s3d::Track& track, const fixed::Vec3& world_pos, const fixed::Vec3& next_world_pos, CollisionSample* out);
  int16_t plane_signed_distance(int16_t plane_index, int16_t x, int16_t y, int16_t z) const;
  void transform_wheel_travel_to_world(int16_t travel, int16_t wheel_heading_offset, const fixed::Mat3& car_to_world,
                                      int16_t car_yaw, int16_t car_pitch, int16_t car_roll,
                                      fixed::Vec3* wheel_world_travel) const;

  const Plane* planes() const { return planes_.data(); }
  size_t plane_count() const { return planes_.size(); }
  const TrackWall* walls() const { return walls_.data(); }
  size_t wall_count() const { return walls_.size(); }

 private:
  bool loaded_ = false;
  std::vector<Plane> planes_;
  std::vector<TrackWall> walls_;
};

}  // namespace s3d::physics
