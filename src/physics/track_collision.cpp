#include "physics/track_collision.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "assets/compression.h"
#include "assets/res_file.h"
#include "physics/wrap_math.h"

namespace s3d::physics {

namespace {
inline constexpr int kTrackSize = 30;
inline constexpr int kTileSize = 1024;
inline constexpr int kTrackGridLastIndex = kTrackSize - 1;
inline constexpr int16_t kNoPlaneIndex = -1;  // we use -1 for none; reference used 0 differently in some contexts
inline constexpr int16_t kTerrainFlatPlaneIndex = 3;
inline constexpr int16_t kTerrainSlopeDownPlaneIndex = 4;
inline constexpr int16_t kTerrainSlopeUpPlaneIndex = 5;

inline constexpr int16_t kAngleMask = 0x3FF;
inline constexpr int16_t kAngleQuarterTurn = 256;
inline constexpr int16_t kAngleHalfTurn = 512;
inline constexpr int16_t kAngleThreeQuarterTurn = 768;

inline constexpr uint8_t kTerrainRaisedTile = 6;
inline constexpr int kRaisedHeight = 450;
inline constexpr int kGrassHeightShift = 8;
inline constexpr int kGrassHeightMask = 1;
inline constexpr int kNonGrassHeightOffset = 2;

inline constexpr int kTerrainSlope5Angle = 128;  // degrees? reference uses 128 in sintab units context; but values come from reference
inline constexpr int kTerrainSlope2Angle = 128;
inline constexpr int kTerrainSlope3Angle = -640;
inline constexpr int kTerrainSlope4Angle = -384;

inline constexpr uint8_t kTileContinuationSoutheast = 0xFD;
inline constexpr uint8_t kTileContinuationSouth = 0xFE;
inline constexpr uint8_t kTileContinuationEast = 0xFF;
inline constexpr uint8_t kTileEmpty = 0x00;

// PHYSICAL_MODEL constants from reference
enum PhysModel {
  PHYS_MODEL_NONE = -1,
  PHYS_MODEL_START_FINISH = 0,
  PHYS_MODEL_ROAD = 1,
  PHYS_MODEL_SHARP_CORNER = 2,
  PHYS_MODEL_LARGE_CORNER = 3,
  PHYS_MODEL_CHICANE_RIGHT_LEFT = 4,
  PHYS_MODEL_CHICANE_LEFT_RIGHT = 5,
  PHYS_MODEL_SHARP_SPLIT_A = 6,
  PHYS_MODEL_SHARP_SPLIT_B = 7,
  PHYS_MODEL_LARGE_SPLIT_A = 8,
  PHYS_MODEL_LARGE_SPLIT_B = 9,
  PHYS_MODEL_HIGHWAY_ENTRANCE = 10,
  PHYS_MODEL_HIGHWAY = 11,
  PHYS_MODEL_CROSSROAD = 12,
  PHYS_MODEL_RAMP = 16,
  PHYS_MODEL_SOLID_RAMP = 17,
  PHYS_MODEL_ELEVATED_ROAD = 18,
  PHYS_MODEL_ELEVATED_SPAN = 19,
  PHYS_MODEL_SOLID_ROAD = 20,
  PHYS_MODEL_ELEVATED_CORNER = 21,
  PHYS_MODEL_OVERPASS = 22,
  PHYS_MODEL_BANKED_ENTRANCE_B = 23,
  PHYS_MODEL_BANKED_ENTRANCE_A = 24,
  PHYS_MODEL_BANKED_ROAD = 25,
  PHYS_MODEL_BANKED_CORNER = 26,
  PHYS_MODEL_LOOP = 27,
  PHYS_MODEL_TUNNEL = 28,
  PHYS_MODEL_PIPE_ENTRANCE = 29,
  PHYS_MODEL_PIPE = 30,
  PHYS_MODEL_HALF_PIPE = 31,
  PHYS_MODEL_CORKSCREW_UP_DOWN_A = 32,
  PHYS_MODEL_CORKSCREW_UP_DOWN_B = 33,
  PHYS_MODEL_SLALOM = 34,
  PHYS_MODEL_CORKSCREW_LEFT_RIGHT = 35,
  PHYS_MODEL_BARN = 65,
  PHYS_MODEL_GAS_STATION = 66,
  PHYS_MODEL_JOES = 67,
  PHYS_MODEL_OFFICE = 68,
  PHYS_MODEL_WINDMILL = 69,
  PHYS_MODEL_SHIP = 70,
};

inline int16_t sar16(int16_t x, unsigned shift) {
  if (shift >= 16) shift = 16;
  int16_t v = x >> shift;
  if ((x & 0x8000) && shift) {
    v |= (int16_t)(0xFFFF << (16 - shift)) & 0xFFFF;
  }
  return v;
}

inline int16_t word_abs(int16_t v) { return v < 0 ? -v : v; }
inline int16_t clamp_word(int16_t v) { return v & kAngleMask; }  // not exactly; angles use &0x3FF

inline int world_to_cell(int w) {
  int c = sar16((int16_t)(w >> 10), 0);  // rough
  // easier: use float logic via integer: c = w / 1024
  c = w / kTileSize;
  if (c < 0) c = 0;
  if (c >= kTrackSize) c = kTrackSize - 1;
  return c;
}

}  // namespace

TrackCollision::TrackCollision() = default;
TrackCollision::TrackCollision(const std::string& game_pre_path) { load_from_game_pre(game_pre_path); }

bool TrackCollision::load_from_game_pre(const std::string& game_pre_path) {
  std::vector<uint8_t> raw_unpacked;
  if (!s3d::decompress_file(game_pre_path, &raw_unpacked)) {
    return false;
  }
  s3d::ResFile rf;
  if (!rf.load(raw_unpacked.data(), raw_unpacked.size())) {
    return false;
  }
  const auto* plan_res = rf.find("plan");
  const auto* wall_res = rf.find("wall");
  if (!plan_res || !wall_res) {
    return false;
  }

  const uint8_t* p = raw_unpacked.data() + plan_res->offset;
  // decode 536 planes * 34 bytes
  planes_.resize(536);
  size_t off = 0;
  for (size_t i = 0; i < 536 && off + 34 <= plan_res->size; ++i) {
    auto r16 = [&](size_t j) -> int16_t { return (int16_t)(p[off + j] | (p[off + j + 1] << 8)); };
    planes_[i].plane_yz = r16(0);
    planes_[i].plane_xy = r16(2);
    planes_[i].plane_origin.x = r16(4);
    planes_[i].plane_origin.y = r16(6);
    planes_[i].plane_origin.z = r16(8);
    planes_[i].plane_normal.x = r16(10);
    planes_[i].plane_normal.y = r16(12);
    planes_[i].plane_normal.z = r16(14);
    // rotation matrix 9 elements
    for (int j = 0; j < 9; ++j) {
      planes_[i].plane_rotation.m[j] = r16(16 + j * 2);
    }
    off += 34;
  }

  const uint8_t* w = raw_unpacked.data() + wall_res->offset;
  walls_.resize(191);
  off = 0;
  for (size_t i = 0; i < 191 && off + 6 <= wall_res->size; ++i) {
    auto r16w = [&](size_t j) -> int16_t { return (int16_t)(w[off + j] | (w[off + j + 1] << 8)); };
    walls_[i].orientation = r16w(0);
    walls_[i].x = r16w(2);
    walls_[i].z = r16w(4);
    off += 6;
  }

  loaded_ = true;
  return true;
}

int16_t TrackCollision::plane_signed_distance(int16_t plane_index, int16_t x, int16_t y, int16_t z) const {
  if (plane_index < 0 || static_cast<size_t>(plane_index) >= planes_.size()) return y;  // fallback
  const Plane& plane = planes_[plane_index];
  fixed::Vec3 world_origin;
  // terrainHeight logic is outside; caller adjusts? reference adds terrainHeight to plane_origin.y in some contexts
  world_origin = plane.plane_origin;
  fixed::Vec3 rel;
  rel.y = y - world_origin.y;
  if (plane_index < 4) {  // matches reference comment
    return rel.y;
  }
  rel.x = x - world_origin.x;
  rel.z = z - world_origin.z;
  // vec_normalInnerProduct: rel · plane_normal / scale
  int32_t prod = (int32_t)rel.x * plane.plane_normal.x + (int32_t)rel.y * plane.plane_normal.y +
                 (int32_t)rel.z * plane.plane_normal.z;
  int32_t q = prod / 8192;  // NORMAL_INNER_PRODUCT_SCALE
  return (int16_t)q;
}

void TrackCollision::transform_wheel_travel_to_world(int16_t travel, int16_t wheel_heading_offset,
                                                     const fixed::Mat3& car_to_world, int16_t car_yaw,
                                                     int16_t car_pitch, int16_t car_roll,
                                                     fixed::Vec3* wheel_world_travel) const {
  // For now, implement the no-plane/fallback branch similar to reference
  // This is a simplified version for the reduced model integration
  if (travel == 0) {
    *wheel_world_travel = fixed::Vec3{0, 0, 0};
    return;
  }
  fixed::Mat3 heading_rotation;
  fixed::mat_rot_y(wrap_negate(wheel_heading_offset), &heading_rotation);
  const fixed::Vec3 forward{0, 0, travel};
  fixed::Vec3 plane_direction;
  fixed::mat_mul_vector(forward, heading_rotation, &plane_direction);
  fixed::mat_mul_vector(plane_direction, car_to_world, wheel_world_travel);
}
} // namespace
