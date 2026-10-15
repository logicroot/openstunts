#include "physics/track_collision.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "assets/compression.h"
#include "assets/res_file.h"
#include "physics/wrap_math.h"
#include "world/track_objects.h"

namespace s3d::physics {

namespace {

inline constexpr int kTrackSize = s3d::Track::kSize;
inline constexpr int kTileSize = 1024;
inline constexpr int kHalfTile = 512;

inline constexpr int kOrient0 = 0;
inline constexpr int kOrient90 = 0x100;
inline constexpr int kOrient180 = 0x200;
inline constexpr int kOrient270 = 0x300;

// Terrain bytes.
inline constexpr uint8_t kTerrainElevated = 6;
inline constexpr int kElevatedHeight = 450;     // hillHeightConsts[1]
inline constexpr int kGrassHeightOffset = 2;    // non-grass surfaces sit +2
inline constexpr int kNonGrassHeightOffset = 2;

// Physical model ids (trkObjectList[].ss_physicalModel).
enum PhysModel {
  PHYS_SFLINE = 0,
  PHYS_ROAD = 1,
  PHYS_SCORNER = 2,
  PHYS_LCORNER = 3,
  PHYS_CHICANE_RL = 4,
  PHYS_CHICANE_LR = 5,
  PHYS_SSPLIT_A = 6,
  PHYS_SSPLIT_B = 7,
  PHYS_LSPLIT_A = 8,
  PHYS_LSPLIT_B = 9,
  PHYS_HIGH_ENTRANCE = 10,
  PHYS_HIGHWAY = 11,
  PHYS_CROSSROAD = 12,
  PHYS_RAMP = 16,
  PHYS_SOLID_RAMP = 17,
  PHYS_ELEV_ROAD = 18,
  PHYS_ELEV_SPAN = 19,
  PHYS_SOLID_ROAD = 20,
  PHYS_ELEV_CORNER = 21,
  PHYS_OVERPASS = 22,
  PHYS_BANK_ENTRANCE_B = 23,
  PHYS_BANK_ENTRANCE_A = 24,
  PHYS_BANK_ROAD = 25,
  PHYS_BANK_CORNER = 26,
  PHYS_LOOP = 27,
  PHYS_TUNNEL = 28,
  PHYS_PIPE_ENTRANCE = 29,
  PHYS_PIPE = 30,
  PHYS_HALF_PIPE = 31,
  PHYS_CORK_UD_LH = 32,
  PHYS_CORK_UD_RH = 33,
  PHYS_SLALOM = 34,
  PHYS_CORK_LR = 35,
  // 64..74: scenery objects and the "empty" cell (blank handler). Only the
  // buildings up to code_bto_ship (70) emit walls; pine/cactus/tennis/palm use
  // code_bto_blank.
  PHYS_EMPTY = 64,
  PHYS_BARN = 65,
  PHYS_GAS_STATION = 66,
  PHYS_JOES = 67,
  PHYS_OFFICE = 68,
  PHYS_WINDMILL = 69,
  PHYS_SHIP = 70,
  PHYS_PINE = 71,
  PHYS_CACTUS = 72,
  PHYS_TENNIS = 73,
  PHYS_PALM = 74,
};

int16_t s16_wrap(int v) { return static_cast<int16_t>(static_cast<uint16_t>(v & 0xFFFF)); }

int polar_radius(int16_t z, int16_t y) {
  return fixed::polar_radius2d(z, y);
}
int polar_angle_2d(int16_t z, int16_t y) {
  return fixed::polar_angle(z, y);
}

// Hill-road substitution (matches track_shapes.cpp): maps a road/deck item on
// hill terrain (7..10) to the matching hill variant item.
uint8_t subst_hillroad_track(int terrain, uint8_t item) {
  static const uint8_t kPieces[4][6] = {
      {4, 14, 24, 39, 59, 98},
      {5, 15, 25, 36, 56, 95},
      {4, 14, 24, 38, 58, 97},
      {5, 15, 25, 37, 57, 96}};
  static const uint8_t kShapes[4][4] = {
      {182, 186, 190, 194},
      {183, 187, 191, 195},
      {184, 188, 192, 196},
      {185, 189, 193, 197}};
  if (terrain < 7 || terrain > 10) return item;
  const int row = terrain - 7;
  for (int p = 0; p < 6; ++p) {
    if (kPieces[row][p] == item) return kShapes[row][p < 3 ? p : 3];
  }
  return item;
}

// highEntrance longitudinal band boundaries (dseg.asm). The reference indexes
// `highEntrZBounds0[i]` and `highEntrZBounds1[i]`, which are the same array
// offset by one element, so ZBounds0[i] == B[i] and ZBounds1[i] == B[i+1].
constexpr int16_t kHighEntrZBounds[7] = {
    static_cast<int16_t>(65024), static_cast<int16_t>(65202),
    static_cast<int16_t>(65368), 0, 168, 334, 1000};
constexpr int16_t kHighEntrXInnBounds[7] = {0, 0, 0, 0, 0, 120, 120};
constexpr int16_t kHighEntrXOutBounds[7] = {120, 168, 216, 264, 312, 360, 360};

// banked-entrance triangular z adjustments (dseg.asm bkRdEntr_triang_zAdjust).
constexpr int16_t kBkRdEntrZAdjust[4] = {
    static_cast<int16_t>(65285), static_cast<int16_t>(65452), 84, 251};

// Corkscrew left/right (code_bto_corkLr) z band per band region (dseg.asm
// corkLR_negZBound / corkLR_posZBound). Index 0 is the "no band" sentinel.
constexpr int16_t kCorkLrNegZBound[12] = {
    0, -94, -187, -280, -373, -466, -559, -652, -745, -838, -931, -1024};
constexpr int16_t kCorkLrPosZBound[12] = {
    0, 1024, 931, 838, 745, 652, 559, 466, 373, 280, 187, 94};

// Loop surface / base bounds (dseg.asm loopSurface_* / loopBase_*).  The
// original stores each "lower" and "upper" array as one shared block, so the
// upper array is the same data shifted by one entry; keep one array each and
// index [i] / [i + 1].
constexpr int16_t kLoopSurfZ[7] = {0, 224, 389, 449, 389, 224, 0};
constexpr int16_t kLoopSurfX[14] = {-400, -400, -352, -304, -270, -235, -200,
                                    0,    135,  299,  524,  749,  914,  975};
constexpr int16_t kLoopMaxZ = 449;
constexpr int16_t kLoopBaseZ[7] = {0, 178, 360, 536, 704, 868, 2000};
constexpr int16_t kLoopBaseInnX[7] = {0, -20, -40, -60, -80, -100, -120};
constexpr int16_t kLoopBaseOutX[7] = {400, 361, 320, 276, 226, 174, 120};

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
  // NOTE: chunk offsets are relative to `rf.data_offset()`, so read() must be
  // used (or data_offset_ added). Reading `raw_unpacked.data() + offset`
  // directly starts 6 + chunks*8 bytes too early.
  const std::vector<uint8_t> plan = rf.read("plan");
  const std::vector<uint8_t> wall = rf.read("wall");
  if (plan.empty() || wall.empty()) {
    return false;
  }

  // 536 planes * 34 bytes.
  planes_.resize(plan.size() / 34);
  for (size_t i = 0; i < planes_.size(); ++i) {
    const size_t off = i * 34;
    auto r16 = [&](size_t j) -> int16_t {
      return static_cast<int16_t>(plan[off + j] | (plan[off + j + 1] << 8));
    };
    planes_[i].plane_yz = r16(0);
    planes_[i].plane_xy = r16(2);
    planes_[i].plane_origin.x = r16(4);
    planes_[i].plane_origin.y = r16(6);
    planes_[i].plane_origin.z = r16(8);
    planes_[i].plane_normal.x = r16(10);
    planes_[i].plane_normal.y = r16(12);
    planes_[i].plane_normal.z = r16(14);
    // Stored as the original's MATRIX, whose array is column-major
    // (_11,_21,_31,_12,...). Our Mat3 is row-major, so transpose on load.
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        planes_[i].plane_rotation.m[row * 3 + col] = r16(16 + (col * 3 + row) * 2);
      }
    }
  }

  // 191 walls * 6 bytes.
  walls_.resize(wall.size() / 6);
  for (size_t i = 0; i < walls_.size(); ++i) {
    const size_t off = i * 6;
    auto r16w = [&](size_t j) -> int16_t {
      return static_cast<int16_t>(wall[off + j] | (wall[off + j + 1] << 8));
    };
    walls_[i].orientation = r16w(0);
    walls_[i].x = r16w(2);
    walls_[i].z = r16w(4);
  }

  loaded_ = true;
  return true;
}

int TrackCollision::plane_signed_distance(int plane_index, int x, int y, int z, int terrain_height,
                                          int elem_x_center, int elem_z_center) const {
  if (plane_index < 0 || static_cast<size_t>(plane_index) >= planes_.size()) {
    // No plane: the reference treats the surface as the flat ground plane at
    // the accumulated terrain height.
    return y - terrain_height;
  }
  const Plane& plane = planes_[plane_index];
  const int by = plane.plane_origin.y + terrain_height;
  const int ay = y - by;
  if (plane_index < 4) {
    return ay;
  }
  const int bx = plane.plane_origin.x + elem_x_center;
  const int bz = plane.plane_origin.z + elem_z_center;
  const int ax = x - bx;
  const int az = z - bz;
  return fixed::vec_normal_inner_product(ax, ay, az, plane.plane_normal);
}

void TrackCollision::plane_push_out(int plane_index, int16_t distance,
                                    fixed::Vec3* out) const {
  if (out == nullptr) return;
  // seg001.asm loc_15C04..loc_15CBE. The correction starts as the vertical
  // vector (0, -distance << 6, 0): `distance` is in word units and the wheel
  // positions are in 1/64-word units, so the shift converts between them. The
  // original performs both the negate and the shift in 16 bits, so keep the
  // int16 wraparound. It is then rotated by the active plane's rotation matrix
  // (the same matrix `transform_wheel_travel_to_world` uses), turning the push
  // into the plane's own up axis. Flat planes have an identity rotation, so the
  // result there is exactly the vertical vector.
  constexpr int kWorldToLongShift = 6;
  const fixed::Vec3 local{
      0,
      static_cast<int16_t>(static_cast<uint16_t>(wrap_negate(distance)) << kWorldToLongShift),
      0};
  if (plane_index < 0 || static_cast<size_t>(plane_index) >= planes_.size()) {
    *out = local;
    return;
  }
  fixed::mat_mul_vector(local, planes_[plane_index].plane_rotation, out);
}

void TrackCollision::transform_wheel_travel_to_world(int16_t travel, int16_t wheel_heading,
                                                     const fixed::Mat3& car_to_world, int16_t car_yaw,
                                                     int16_t car_pitch, int16_t car_roll,
                                                     int plane_index,
                                                     fixed::Vec3* wheel_world_travel) const {
  if (wheel_world_travel == nullptr) return;
  const fixed::Vec3 travel_vec{0, 0, travel};

  if (plane_index < 0 || static_cast<size_t>(plane_index) >= planes_.size()) {
    // No-plane branch (math.c loc_198C2): car_to_world * R_y(-heading) * travel.
    if (wheel_heading == 0) {
      fixed::mat_mul_vector(travel_vec, car_to_world, wheel_world_travel);
      return;
    }
    fixed::Mat3 heading_rot;
    fixed::mat_rot_y(wrap_negate(wheel_heading), &heading_rot);
    fixed::Vec3 rotated;
    fixed::mat_mul_vector(travel_vec, heading_rot, &rotated);
    fixed::mat_mul_vector(rotated, car_to_world, wheel_world_travel);
    return;
  }

  const Plane& plane = planes_[plane_index];
  int si;
  if (plane.plane_xy == car_pitch && plane.plane_yz == car_roll) {
    si = car_yaw;
  } else {
    // var_32 = R_plane^-1 * (car_to_world * travel).
    fixed::Vec3 in_car;
    fixed::mat_mul_vector(travel_vec, car_to_world, &in_car);
    fixed::Mat3 plane_inv;
    fixed::mat_invert(plane.plane_rotation, &plane_inv);
    fixed::Vec3 in_plane;
    fixed::mat_mul_vector(in_car, plane_inv, &in_plane);
    si = polar_angle_2d(wrap_negate(in_plane.x), in_plane.z);
  }
  si = wrap_add(static_cast<int16_t>(si), wheel_heading);

  fixed::Vec3 plane_dir;
  if (si == 0) {
    plane_dir = travel_vec;
  } else {
    fixed::Mat3 rot;
    fixed::mat_rot_y(wrap_negate(si), &rot);
    fixed::mat_mul_vector(travel_vec, rot, &plane_dir);
  }
  fixed::mat_mul_vector(plane_dir, plane.plane_rotation, wheel_world_travel);
}

void TrackCollision::build_track_object_from_track(const s3d::Track& track,
                                                   const fixed::Vec3& world_pos,
                                                   const fixed::Vec3& next_world_pos,
                                                   CollisionSample* out) const {
  if (out == nullptr) return;
  CollisionSample sample;
  sample.plane_index = 0;
  sample.wall_index = -1;
  sample.surface_type = static_cast<int8_t>(CarSurfaceType::Grass);
  sample.wall_height = -12;  // seg004.asm: wallHeight = 0FFF4h
  sample.wall_lower_bound = -1000;

  if (!loaded_) {
    *out = sample;
    return;
  }

  const int16_t world_x = world_pos.x;
  const int16_t world_z = world_pos.z;

  const int col = static_cast<int>(world_x) >> 10;
  const int row = static_cast<int>(world_z) >> 10;
  if (col < 0 || col >= kTrackSize || row < 0 || row >= kTrackSize) {
    *out = sample;
    return;
  }

  int terrain_height = 0;
  const uint8_t terrain = track.terrain_at(col, row);
  if (terrain == kTerrainElevated) terrain_height = kElevatedHeight;

  // Resolve the element covering the cell (continuation markers point back at
  // their multi-tile owner).
  world::PlacedTrackObject placed;
  uint8_t item = 0;
  int owner_col = col;
  int owner_row = row;
  if (world::track_object_at(track, col, row, &placed)) {
    item = static_cast<uint8_t>(placed.item);
    owner_col = placed.owner_x;
    owner_row = placed.owner_y;
  }

  int8_t surface_type = static_cast<int8_t>(CarSurfaceType::Grass);
  if (item == 0) {
    // No element. Water terrain reads as water; otherwise grass.
    if (terrain >= 1 && terrain <= 5) surface_type = static_cast<int8_t>(CarSurfaceType::Water);
  } else {
    item = subst_hillroad_track(terrain, item);
  }

  const world::TrackObjectDef& def = world::track_object(item == 0 ? 0 : item);
  int16_t elem_x_center;
  int16_t elem_z_center;
  if (item == 0) {
    elem_x_center = static_cast<int16_t>(col * kTileSize + kHalfTile);
    elem_z_center = static_cast<int16_t>(row * kTileSize + kHalfTile);
  } else {
    elem_x_center = (def.multi_tile & 2) ? static_cast<int16_t>((owner_col + 1) * kTileSize)
                                         : static_cast<int16_t>(owner_col * kTileSize + kHalfTile);
    elem_z_center = (def.multi_tile & 1) ? static_cast<int16_t>(owner_row * kTileSize)
                                         : static_cast<int16_t>(owner_row * kTileSize + kHalfTile);
  }

  const int16_t raw_vx = wrap_sub(world_x, elem_x_center);
  const int16_t raw_vz = wrap_sub(world_z, elem_z_center);
  const int16_t raw_nvx = wrap_sub(next_world_pos.x, elem_x_center);
  const int16_t raw_nvz = wrap_sub(next_world_pos.z, elem_z_center);

  int16_t vx = raw_vx;
  int16_t vz = raw_vz;
  int16_t nvx = raw_nvx;
  int16_t nvz = raw_nvz;

  int16_t elem_orient = kOrient0;
  int16_t wall_orient_mod = 0;
  int planindex = 0;
  int wallindex = -1;
  int wall_height = -12;  // seg004.asm: wallHeight = 0FFF4h
  int wall_lower_bound = -1000;
  const int16_t phys = item == 0 ? -1 : def.phys;

  if (item != 0) {
    elem_orient = def.rot_y;

    // Rotate the element-relative coordinates into the canonical orientation.
    auto rotate = [&](int16_t* px, int16_t* pz) {
      const int16_t ox = *px;
      const int16_t oz = *pz;
      switch (elem_orient) {
        case kOrient90:  // loc_1E562
          *px = wrap_negate(oz);
          *pz = ox;
          break;
        case kOrient180:  // loc_1E540
          *px = wrap_negate(ox);
          *pz = wrap_negate(oz);
          break;
        case kOrient270:  // loc_1E516
          *px = oz;
          *pz = wrap_negate(ox);
          break;
        default:
          break;
      }
    };
    rotate(&vx, &vz);
    rotate(&nvx, &nvz);
  }

  const int16_t abs_x = vx < 0 ? wrap_negate(vx) : vx;
  const int16_t abs_z = vz < 0 ? wrap_negate(vz) : vz;

  // Dispatch on the physical model. Each branch fills planindex / surface /
  // walls; unclear or unsupported models fall through to code_bto_blank.
  auto set_pavement = [&]() { surface_type = static_cast<int8_t>(def.surface) + 1; };

  if (item != 0 && phys >= 0 && phys <= 74) {
    // The sharp/large corner radius tests share the same "pavement if the turn
    // radius falls inside the road band" shape.
    auto sharp_corner = [&](int16_t cx, int16_t cz) {
      const int turn = polar_radius(cx, cz);
      if (turn > 0x188 && turn < 0x278) set_pavement();
    };
    auto large_corner = [&](int16_t cx, int16_t cz) {
      const int turn = polar_radius(cx, cz);
      if (turn > 0x588 && turn < 0x678) set_pavement();
    };

    switch (phys) {
      case PHYS_SFLINE:
      case PHYS_ROAD: {
        if (abs_x < 0x78) set_pavement();
        break;
      }
      case PHYS_CROSSROAD: {
        if (abs_x < 0x78 || abs_z < 0x78) set_pavement();
        break;
      }
      case PHYS_CHICANE_LR: {
        vx = wrap_negate(vx);
        nvx = wrap_negate(nvx);
        [[fallthrough]];
      }
      case PHYS_CHICANE_RL: {
        set_pavement();
        if (vx > 0) {
          vz = wrap_negate(vz);
          vx = wrap_negate(vx);
        }
        [[fallthrough]];
      }
      case PHYS_LCORNER: {
        large_corner(wrap_add(vz, 0x400), wrap_add(vx, 0x400));
        break;
      }
      case PHYS_SCORNER: {
        sharp_corner(wrap_add(vz, 0x200), wrap_add(vx, 0x200));
        break;
      }
      case PHYS_SSPLIT_A: {
        if (abs_x < 0x78) {
          set_pavement();
        } else {
          sharp_corner(wrap_add(vz, 0x200), wrap_add(vx, 0x200));
        }
        break;
      }
      case PHYS_SSPLIT_B: {
        if (abs_x < 0x78) {
          set_pavement();
        } else {
          sharp_corner(wrap_add(vz, 0x200), wrap_add(0x200, wrap_negate(vx)));
        }
        break;
      }
      case PHYS_LSPLIT_A: {
        if (vx >= 0x188 && vx <= 0x278) {
          set_pavement();
        } else {
          large_corner(wrap_add(vz, 0x400), wrap_add(vx, 0x400));
        }
        break;
      }
      case PHYS_LSPLIT_B: {
        if (vx >= static_cast<int16_t>(0xFD88) && vx <= static_cast<int16_t>(0xFE78)) {
          set_pavement();
        } else {
          large_corner(wrap_add(vz, 0x400), wrap_add(0x400, wrap_negate(vx)));
        }
        break;
      }
      case PHYS_HIGH_ENTRANCE: {
        // The entrance tapers: the drivable band runs from an inner to an outer
        // x bound that both vary along z. Walk the band table until the z upper
        // bound covers vz, then interpolate both bounds across the band.
        const int16_t misc1c = static_cast<int16_t>(vx < 0 ? wrap_negate(vx) : vx);
        int si = 0;
        while (si < 5 && kHighEntrZBounds[si + 1] < vz) ++si;
        auto interp = [&](const int16_t* bounds) -> int {
          const int z0 = kHighEntrZBounds[si];
          const int z1 = kHighEntrZBounds[si + 1];
          const int b0 = bounds[si];
          const int b1 = bounds[si + 1];
          if (b1 == b0) return b0;
          if (z1 == z0) return b0;
          return (static_cast<int>(vz) - z0) * (b1 - b0) / (z1 - z0) + b0;
        };
        const int inner = interp(kHighEntrXInnBounds);
        const int outer = interp(kHighEntrXOutBounds);
        if (misc1c > inner && misc1c < outer) {
          set_pavement();
          break;
        }
        if (vz < 0) break;
        if (misc1c > 0x78) break;
        planindex = 1;
        if (vz >= 0x14E) {
          if (nvx <= static_cast<int16_t>(0xFF88)) {
            wallindex = 0xBC;
          } else if (nvx >= 0x78) {
            wallindex = 0xBA;
          }
        } else if (nvx < 0) {
          wallindex = 0xBD;
        } else {
          wallindex = 0xBB;
        }
        break;
      }
      case PHYS_HIGHWAY: {
        if (abs_x > 0x168) break;
        if (abs_x > 0x78) {
          set_pavement();
          break;
        }
        planindex = 1;
        if (nvx > static_cast<int16_t>(0xFF88) && nvx < 0x78) {
          wallindex = 0xBA;
        } else if (nvx > static_cast<int16_t>(0xFF88)) {
          wallindex = 0xBC;
        } else {
          wallindex = 0xBC;
        }
        break;
      }
      case PHYS_RAMP:
      case PHYS_SOLID_RAMP: {
        bool hill_side = true;  // byte_4392C
        if (phys == PHYS_RAMP) {
          if (vz > 0) {
            hill_side = false;
          } else if (nvz >= 0) {
            wallindex = 0x66;
          }
        } else if (nvz >= 0x1DC) {
          wallindex = 0x67;
        }
        const int16_t absnvx = nvx < 0 ? wrap_negate(nvx) : nvx;
        if (absnvx < 0x78) {
          planindex = 3;
          set_pavement();
          if (wallindex != -1) break;
          if (vz < 0) break;
          if (abs_x < 0x78) break;
          wall_height = 0x2A;
          wall_lower_bound = -12;
          wallindex = vx >= 0 ? 0x65 : 0x64;
        } else {
          if (!hill_side) break;
          if (abs_x > 0x78) break;
          planindex = 3;
          if (wallindex != -1) break;
          wall_orient_mod = kOrient180;
          wallindex = vx >= 0 ? 0x65 : 0x64;
        }
        break;
      }
      case PHYS_OVERPASS:
      case PHYS_SOLID_ROAD:
      case PHYS_ELEV_ROAD:
      case PHYS_ELEV_SPAN: {
        bool deck = true;  // byte_4392C
        if (phys == PHYS_OVERPASS) {
          if (static_cast<int>(world_pos.y) - terrain_height <= 0x186) {
            if (abs_z <= 0x78) set_pavement();
            break;
          }
          deck = false;
        } else if (phys == PHYS_ELEV_ROAD || phys == PHYS_ELEV_SPAN) {
          if (static_cast<int>(world_pos.y) - terrain_height <= 0x186) break;
          deck = false;
        }
        const int16_t absnvx = nvx < 0 ? wrap_negate(nvx) : nvx;
        if (absnvx <= 0x78) {
          planindex = 2;
          set_pavement();
          if (deck) {
            if (nvz >= 0x1DC) {
              wallindex = 0x67;
            } else if (nvz <= static_cast<int16_t>(0xFE24)) {
              wallindex = 0x68;
            }
          }
          if (abs_x < 0x78) break;
          wall_height = 0x2A;
          wallindex = vx >= 0 ? 0x65 : 0x64;
        } else {
          if (!deck) break;
          if (abs_x > 0x78) break;
          planindex = 2;
          wall_height = 0x2A;
          wall_orient_mod = kOrient180;
          wallindex = nvx >= 0 ? 0x65 : 0x64;
        }
        break;
      }
      case PHYS_ELEV_CORNER: {
        if (static_cast<int>(world_pos.y) - terrain_height <= 0x186) break;
        const int turn = polar_radius(wrap_add(vz, 0x400), wrap_add(vx, 0x400)) - 0x600;
        if (turn <= static_cast<int16_t>(0xFF6A)) break;
        if (turn >= 0x96) break;
        set_pavement();
        planindex = 2;
        if (turn >= static_cast<int16_t>(0xFF94) && turn <= 0x6C) break;
        const int angle = polar_angle_2d(wrap_add(vz, 0x400), wrap_add(vx, 0x400));
        int wallsel = (s16_wrap((angle & 0xFF) * 0x12) >> 8);
        wallsel = -(wallsel - 0x11);
        wall_height = 0x2A;
        wall_lower_bound = -12;
        wallindex = wrap_add(static_cast<int16_t>(wallsel), turn < 0 ? 0x69 : 0x7B);
        break;
      }
      case PHYS_BANK_ENTRANCE_A:
      case PHYS_BANK_ENTRANCE_B: {
        int misc1c = (phys == PHYS_BANK_ENTRANCE_A) ? 0x23 : 0x19;
        int misc1e = (phys == PHYS_BANK_ENTRANCE_A) ? 0 : 1;
        const int16_t si = (phys == PHYS_BANK_ENTRANCE_A) ? static_cast<int16_t>(0xFD60)
                                                          : static_cast<int16_t>(0xA0);
        if (abs_x > 0x78) break;
        if (misc1e == 0 && nvx <= static_cast<int16_t>(0xFF88)) {
          wall_orient_mod = kOrient180;
          wallindex = 0x64;
        } else if (misc1e != 0 && nvx >= 0x78) {
          wall_orient_mod = kOrient180;
          wallindex = 0x65;
        }
        set_pavement();
        const int16_t zz = vz;
        if (zz < static_cast<int16_t>(0xFEB2)) {
          planindex = misc1c;
        } else if (zz >= 0x14E) {
          planindex = misc1c + 9;
        } else if (zz < static_cast<int16_t>(0xFF58)) {
          planindex = misc1c + 1;
          misc1e = 0;
        } else if (zz < 0) {
          planindex = misc1c + 3;
          misc1e = 1;
        } else if (zz < 0xA8) {
          planindex = misc1c + 5;
          misc1e = 2;
        } else if (zz < 0x14E) {
          planindex = misc1c + 7;
          misc1e = 3;
        }
        const int16_t adj = wrap_sub(vz, kBkRdEntrZAdjust[misc1e]);
        const int proj = fixed::multiply_and_scale(fixed::sin_fast(static_cast<uint16_t>(si)), adj) +
                         fixed::multiply_and_scale(fixed::cos_fast(static_cast<uint16_t>(si)), vx);
        if (proj > 0) ++planindex;
        break;
      }
      case PHYS_BANK_ROAD: {
        if (abs_x > 0x78) break;
        set_pavement();
        planindex = 6;
        if (nvx < 0x78) break;
        wall_orient_mod = kOrient180;
        wallindex = 0x65;
        break;
      }
      case PHYS_BANK_CORNER: {
        const int turn = polar_radius(wrap_add(vz, 0x400), wrap_add(vx, 0x400)) - 0x600;
        if (turn <= static_cast<int16_t>(0xFF88)) break;
        if (turn >= 0x7E) break;
        const int angle = polar_angle_2d(wrap_add(vz, 0x400), wrap_add(vx, 0x400));
        int wallsel = (s16_wrap((angle & 0xFF) * 0x12) >> 8);
        wallsel = -(wallsel - 0x11);
        planindex = wallsel + 7;
        set_pavement();
        if (turn > 0x66) {
          wall_orient_mod = kOrient180;
          wallindex = wallsel + 0x7B;
        }
        break;
      }
      // ---- Loop (code_bto_loop / code_bto_loopBase) ----------------------
      case PHYS_LOOP: {
        auto lerp = [](int v, int z0, int z1, int b0, int b1) -> int {
          if (z1 == z0) return b0;
          return (v - z0) * (b1 - b0) / (z1 - z0) + b0;
        };
        // loopBase is entered directly by the loop handler; it only reports
        // whether the wheel sits on the base pavement.
        auto loop_base = [&](int z2e, int x6e) -> bool {
          int si = 0;
          while (si < 5 && kLoopBaseZ[si + 1] < z2e) ++si;
          const int inner = lerp(z2e, kLoopBaseZ[si], kLoopBaseZ[si + 1],
                                 kLoopBaseInnX[si], kLoopBaseInnX[si + 1]);
          const int outer = lerp(z2e, kLoopBaseZ[si], kLoopBaseZ[si + 1],
                                 kLoopBaseOutX[si], kLoopBaseOutX[si + 1]);
          return x6e >= inner && x6e <= outer;
        };

        const bool neg = vz < 0;
        const int16_t x6e = neg ? wrap_negate(vx) : vx;
        const int16_t z2e = neg ? wrap_negate(vz) : vz;
        if (z2e > kLoopMaxZ + 0x64) {
          if (loop_base(z2e, x6e)) set_pavement();
          break;
        }
        const int16_t m1e =
            z2e <= kLoopMaxZ - 1 ? z2e : static_cast<int16_t>(kLoopMaxZ - 1);
        int si = 0;
        while (si < 5 && kLoopSurfZ[si + 1] < m1e) ++si;
        const int lower_plan = (neg ? 0x33 : 0x2D) + si;

        const int height = static_cast<int>(world_pos.y) - terrain_height;
        if (height > 0x20C) {
          // Upper half: the band index is mirrored before the x test.
          si = 5 - si;
          bool blank = false;
          bool found = false;
          if (kLoopSurfX[si] > x6e) {
            blank = true;
          } else if (kLoopSurfX[si + 1] + 0x190 < x6e) {
            blank = true;
          } else if (kLoopSurfX[si + 1] < x6e) {
            found = kLoopSurfX[si] + 0x190 > x6e;
          }
          if (!blank && !found) {
            const int m3c = lerp(m1e, kLoopSurfZ[si], kLoopSurfZ[si + 1],
                                 kLoopSurfX[si], kLoopSurfX[si + 1]);
            found = m3c < x6e && m3c + 0x190 > x6e;
          }
          if (found) {
            planindex = (neg ? 0x33 : 0x2D) + si;
            set_pavement();
          }
        } else {
          if (si > 1 && height < 0x64) {
            if (loop_base(z2e, x6e)) set_pavement();
            break;
          }
          bool base = false;
          bool found = false;
          if (kLoopSurfX[si] > x6e) {
            base = true;
          } else if (kLoopSurfX[si + 1] + 0x190 < x6e) {
            base = true;
          } else if (kLoopSurfX[si + 1] < x6e) {
            found = kLoopSurfX[si] + 0x190 > x6e;
          }
          if (!base && !found) {
            if (kLoopSurfX[si + 1] == kLoopSurfX[si]) {
              base = true;
            } else {
              const int m3c = lerp(m1e, kLoopSurfZ[si], kLoopSurfZ[si + 1],
                                   kLoopSurfX[si], kLoopSurfX[si + 1]);
              if (m3c >= x6e || m3c + 0x190 <= x6e) {
                base = true;
              } else {
                found = true;
              }
            }
          }
          if (found) {
            planindex = lower_plan;
            set_pavement();
          } else if (base && loop_base(z2e, x6e)) {
            set_pavement();
          }
        }
        break;
      }
      // ---- Tunnel (code_bto_tunnel) --------------------------------------
      case PHYS_TUNNEL: {
        const int height = static_cast<int>(world_pos.y) - terrain_height;
        const int next_height = static_cast<int>(next_world_pos.y) - terrain_height;
        if (height >= 0x90 || next_height >= 0x90) {
          // Inside the bore: a flat road, unless the wheel leaves laterally.
          if (abs_x >= 0x10E) break;
          set_pavement();
          planindex = 0x85;
          break;
        }
        if (abs_x < 0x78) set_pavement();
        if (vx >= 0x78 && vx <= 0x10E) {
          wall_height = 0x90;
          if (nvz <= static_cast<int16_t>(0xFE00)) {
            wallindex = 0x9A;
          } else if (nvz >= 0x200) {
            wallindex = 0x99;
          } else if (nvx <= 0x78) {
            wallindex = 0x98;
          } else if (nvx >= 0x10E) {
            wallindex = 0x96;
          }
          break;
        }
        if (vx > static_cast<int16_t>(0xFF88)) break;
        if (vx < static_cast<int16_t>(0xFEF2)) break;
        wall_height = 0x90;
        if (nvz <= static_cast<int16_t>(0xFE00)) {
          wallindex = 0x9A;
        } else if (nvz >= 0x200) {
          wallindex = 0x99;
        } else if (nvx <= static_cast<int16_t>(0xFF88)) {
          wallindex = 0x97;
        } else if (nvx <= static_cast<int16_t>(0xFEF2)) {
          wallindex = 0x95;
        }
        break;
      }
      // ---- Pipe entrance (code_bto_pipeEntrance) --------------------------
      case PHYS_PIPE_ENTRANCE: {
        const int16_t abs_nvx = nvx < 0 ? wrap_negate(nvx) : nvx;
        if (abs_nvx >= 0x73 && abs_x <= 0xA4) {
          wall_height = 0x97;
          wallindex = nvx > 0 ? 0x9F : 0xA0;
          break;
        }
        if (abs_x >= 0x73) break;
        const int height = static_cast<int>(world_pos.y) - terrain_height;
        if (height >= 0xAB) break;
        set_pavement();
        if (abs_x < 0x1F) {
          planindex = 0x46;
          break;
        }
        int16_t m1e;
        int si;
        if (vx < static_cast<int16_t>(0xFFAC)) {
          planindex = 0x49;
          m1e = static_cast<int16_t>(0xFF9C);
          si = -5;
        } else if (vx < 0) {
          planindex = 0x47;
          m1e = static_cast<int16_t>(0xFFC7);
          si = -8;
        } else if (vx > 0x54) {
          planindex = 0x4D;
          m1e = 0x64;
          si = 5;
        } else {
          planindex = 0x4B;
          m1e = 0x39;
          si = 8;
        }
        const int proj =
            fixed::multiply_and_scale(fixed::sin_fast(static_cast<uint16_t>(si)), vz) +
            fixed::multiply_and_scale(fixed::cos_fast(static_cast<uint16_t>(si)),
                                      wrap_sub(vx, m1e));
        if (s16_wrap(proj) < 0) ++planindex;
        break;
      }
      // ---- Pipe / half pipe (code_bto_halfPipe / code_bto_pipe) ----------
      case PHYS_HALF_PIPE:
      case PHYS_PIPE: {
        const int16_t abs_nvx = nvx < 0 ? wrap_negate(nvx) : nvx;
        if (abs_nvx >= 0xA4 && abs_x <= 0xA4) {
          wall_height = 0x97;
          wallindex = nvx > 0 ? 0x9B : 0x9C;
          break;
        }
        if (abs_x >= 0xA4) break;
        const int height = static_cast<int>(world_pos.y) - terrain_height;
        if (height >= 0x109) break;
        if (abs_x < 0x82) set_pavement();
        const bool high = height > 0x97;
        if (phys == PHYS_HALF_PIPE && !high && abs_x <= 0x54 && abs_z <= 0x4B) {
          planindex = 0x45;
          if (nvz <= static_cast<int16_t>(0xFFB5)) {
            wallindex = 0x9D;
          } else if (nvz >= 0x4B) {
            wallindex = 0x9E;
          }
          break;
        }
        if (height > 0x58 && !high) {
          planindex = vx < 0 ? 0x3C : 0x42;
          break;
        }
        if (abs_x < 0x1F) {
          planindex = high ? 0x3F : 0x39;
          break;
        }
        if (vx < static_cast<int16_t>(0xFFAC)) {
          planindex = high ? 0x3D : 0x3B;
          break;
        }
        if (vx < 0) {
          planindex = high ? 0x3E : 0x3A;
          break;
        }
        if (vx > 0x54) {
          planindex = high ? 0x41 : 0x43;
          break;
        }
        planindex = high ? 0x40 : 0x44;
        break;
      }
      // ---- Corkscrew left/right (code_bto_corkLr) ------------------------
      case PHYS_CORK_LR: {
        if (abs_x >= 0x96) break;
        const int height = static_cast<int>(world_pos.y) - terrain_height;
        if (height >= 0x109) break;
        set_pavement();
        const bool high = height > 0x97;
        // Region 0 means "no band": the spiral is not under the wheel yet.
        int region = 0;
        if (height > 0x58 && !high) {
          region = vx < 0 ? 3 : 9;
        } else if (abs_x < 0x1F) {
          if (high) region = 6;
        } else if (vx < static_cast<int16_t>(0xFFAC)) {
          region = high ? 4 : 2;
        } else if (vx < 0) {
          region = high ? 5 : 1;
        } else if (vx > 0x54) {
          region = high ? 8 : 0xA;
        } else {
          region = high ? 7 : 0xB;
        }
        if (region != 0 && kCorkLrNegZBound[region] < vz &&
            kCorkLrPosZBound[region] > vz) {
          planindex = region + 0x39;
        }
        if (planindex != 0) break;
        if (abs_z >= 0x200) break;
        wall_height = 0x75;
        wallindex = 0xB9;
        break;
      }
      // ---- Corkscrew up/down (code_bto_corkUdLH / code_bto_corkUdRH) ------
      case PHYS_CORK_UD_LH:
      case PHYS_CORK_UD_RH: {
        const bool rh = phys == PHYS_CORK_UD_RH;
        const int16_t misc1e = rh ? vx : wrap_negate(vx);
        const int misc22 = rh ? 0x69 : 0x4F;
        const int var_c = rh ? 0x00 : 0x32;
        const int var_e = rh ? 0x19 : 0x4B;
        const int height = static_cast<int>(world_pos.y) - terrain_height;
        if (vz < 0 && height < 0x64 && misc1e > 0) {
          if (misc1e >= 0x278) break;
          if (misc1e <= 0x188) break;
          set_pavement();
          planindex = misc22;
          break;
        }
        if (vz > 0 && height > 0x15E && misc1e < 0x2B4 && misc1e > 0x14C) {
          wall_height = 0x2A;
          wall_lower_bound = -12;
          wallindex = (misc1e > 0x200 ? var_c : var_e) + 0x18;
          set_pavement();
          planindex = misc22 + 0x19;
          break;
        }
        const int turn = polar_radius(vz, misc1e);
        if (turn <= 0x14C) break;
        if (turn >= 0x2B4) break;
        int16_t angle = static_cast<int16_t>(polar_angle_2d(vz, misc1e));
        angle = wrap_negate(wrap_sub(angle, 0x100));
        angle = static_cast<int16_t>(angle & 0x3FF);
        const int si = s16_wrap(angle * 0x18) >> 10;
        planindex = misc22 + si + 1;
        set_pavement();
        wall_height = 0x2A;
        wall_lower_bound = -12;
        const int del = turn - 0x200;
        if (del > 0x5A) {
          wallindex = var_c + si;
        } else if (del < static_cast<int16_t>(0xFFA6)) {
          break;
        } else {
          wallindex = var_e + si;
        }
        break;
      }
      // ---- Slalom (code_bto_slalom) --------------------------------------
      case PHYS_SLALOM: {
        if (abs_x < 0x78) set_pavement();
        // Gate post lying across z: inner x band and a narrow z band.
        if (vx >= 0x17 && vx <= 0x61 && vz > static_cast<int16_t>(0xFEF1) &&
            vz < static_cast<int16_t>(0xFF0F)) {
          wall_height = 0x2A;
          if (nvz < static_cast<int16_t>(0xFEF1)) {
            wallindex = 0x91;
          } else if (nvz > static_cast<int16_t>(0xFF0F)) {
            wallindex = 0x92;
          } else if (nvx < 0x17) {
            wallindex = 0x94;
          } else if (nvx > 0x61) {
            wallindex = 0x93;
          }
          break;
        }
        // Gate post lying across x (the other slalom orientation).
        if (vx > static_cast<int16_t>(0xFFE9)) break;
        if (vx < static_cast<int16_t>(0xFF9F)) break;
        if (vz >= 0x10F) break;
        if (vz <= 0xF1) break;
        wall_height = 0x2A;
        if (nvz > 0x10F) {
          wallindex = 0x8D;
        } else if (nvz < 0xF1) {
          wallindex = 0x8E;
        } else if (nvx > static_cast<int16_t>(0xFFE9)) {
          wallindex = 0x8F;
        } else if (nvx < static_cast<int16_t>(0xFF9F)) {
          wallindex = 0x90;
        }
        break;
      }
      // ---- Scenery buildings ---------------------------------------------
      // Each is a rectangular footprint that rejects the wheel unless it is
      // inside, then picks the wall facing the wheel's next position.
      case PHYS_BARN: {
        if (abs_x > 0x96 || abs_z > 0x96) break;
        wall_height = 0x1A9;
        if (nvz <= static_cast<int16_t>(0xFF6A)) {
          wallindex = 0xA1;
        } else if (nvz >= 0x96) {
          wallindex = 0xA2;
        } else if (nvx >= 0x96) {
          wallindex = 0xA3;
        } else if (nvx <= static_cast<int16_t>(0xFF6A)) {
          wallindex = 0xA4;
        }
        break;
      }
      case PHYS_GAS_STATION: {
        if (vx < static_cast<int16_t>(0xFF38) || vx > 0x104 || abs_z > 0x50) break;
        wall_height = 0xE6;
        if (nvz <= static_cast<int16_t>(0xFFB0)) {
          wallindex = 0xA5;
        } else if (nvz >= 0x50) {
          wallindex = 0xA8;
        } else if (nvx <= static_cast<int16_t>(0xFF38)) {
          wallindex = 0xA6;
        } else if (nvx >= 0x104) {
          wallindex = 0xA7;
        }
        break;
      }
      case PHYS_JOES: {
        if (abs_x > 0xB4 || abs_z > 0x64) break;
        wall_height = 0xF8;
        if (nvz <= static_cast<int16_t>(0xFF9C)) {
          wallindex = 0xA9;
        } else if (nvz >= 0x64) {
          wallindex = 0xAC;
        } else if (nvx <= static_cast<int16_t>(0xFF4C)) {
          wallindex = 0xAB;
        } else if (nvx >= 0xB4) {
          wallindex = 0xAA;
        }
        break;
      }
      case PHYS_OFFICE: {
        if (abs_x > 0xC8 || abs_z > 0xC8) break;
        wall_height = 0x226;
        if (nvz <= static_cast<int16_t>(0xFF38)) {
          wallindex = 0xAD;
        } else if (nvz >= 0xC8) {
          wallindex = 0xAE;
        } else if (nvx <= static_cast<int16_t>(0xFF38)) {
          wallindex = 0xAF;
        } else if (nvx >= 0xC8) {
          wallindex = 0xB0;
        }
        break;
      }
      case PHYS_WINDMILL: {
        if (abs_x > 0x72 || abs_z > 0x72) break;
        wall_height = 0x1EF;
        if (nvz <= static_cast<int16_t>(0xFF8E)) {
          wallindex = 0xB4;
        } else if (nvz >= 0x72) {
          wallindex = 0xB2;
        } else if (nvx <= static_cast<int16_t>(0xFF8E)) {
          wallindex = 0xB1;
        } else if (nvx >= 0x72) {
          wallindex = 0xB3;
        }
        break;
      }
      case PHYS_SHIP: {
        if (vx < static_cast<int16_t>(0xFF56) || vx > 0x104 || abs_z > 0x6E) break;
        wall_height = 0xE6;
        if (nvz <= static_cast<int16_t>(0xFF92)) {
          wallindex = 0xB5;
        } else if (nvz >= 0x6E) {
          wallindex = 0xB8;
        } else if (nvx <= static_cast<int16_t>(0xFF56)) {
          wallindex = 0xB7;
        } else if (nvx >= 0x104) {
          wallindex = 0xB6;
        }
        break;
      }
      default:
        break;
    }
  }

  // code_bto_blank: hill terrain (7..18) selects a slope plane even with no
  // element.
  if (terrain >= 7 && terrain <= 18) {
    // Recompute element-relative coordinates (raw, un-rotated).
    vx = raw_vx;
    vz = raw_vz;
    // Direction variants rotate the coordinate pair.
    const int variant = (terrain - 7) & 3;
    switch (variant) {
      case 0:
        break;
      case 1: {  // loc_1F832
        const int16_t ox = vx;
        vx = vz;
        vz = wrap_negate(ox);
        break;
      }
      case 2: {  // loc_1F84E
        vx = wrap_negate(vx);
        vz = wrap_negate(vz);
        break;
      }
      case 3: {  // loc_1F866
        const int16_t ox = vx;
        vx = wrap_negate(vz);
        vz = ox;
        break;
      }
    }
    switch (terrain) {
      case 7:
      case 8:
      case 9:
      case 10:
        elem_orient = variant == 0 ? kOrient0 : (variant == 1 ? kOrient270 : (variant == 2 ? kOrient180 : kOrient90));
        if (planindex == 0) planindex = 3;
        break;
      case 11:
      case 12:
      case 13:
      case 14: {
        elem_orient = variant == 0 ? kOrient0 : (variant == 1 ? kOrient270 : (variant == 2 ? kOrient180 : kOrient90));
        const int16_t s = static_cast<int16_t>(0xFF80);
        const int proj = fixed::multiply_and_scale(fixed::sin_fast(static_cast<uint16_t>(s)), vz) +
                         fixed::multiply_and_scale(fixed::cos_fast(static_cast<uint16_t>(s)), vx);
        if (proj < 0) planindex = 4;
        break;
      }
      case 15:
      case 16:
      case 17:
      case 18: {
        elem_orient = variant == 0 ? kOrient0 : (variant == 1 ? kOrient270 : (variant == 2 ? kOrient180 : kOrient90));
        const int16_t s = static_cast<int16_t>(0xFF80);
        const int proj = fixed::multiply_and_scale(fixed::sin_fast(static_cast<uint16_t>(s)), vz) +
                         fixed::multiply_and_scale(fixed::cos_fast(static_cast<uint16_t>(s)), vx);
        if (proj > 0) {
          planindex = 5;
        } else {
          terrain_height = kElevatedHeight;
        }
        break;
      }
      default:
        break;
    }
  }

  // Final plane selection (1F8CD..1F9D9).
  int final_plane = 0;
  if (planindex > 0) {
    int base = planindex * 4;
    switch (elem_orient) {
      case kOrient90:
        base += 3;
        break;
      case kOrient180:
        base += 2;
        break;
      case kOrient270:
        base += 1;
        break;
      default:
        break;
    }
    final_plane = base;
  }
  if (final_plane < 0 || static_cast<size_t>(final_plane) >= planes_.size()) final_plane = 0;

  if (surface_type == static_cast<int8_t>(CarSurfaceType::Grass)) {
    terrain_height += ((world_x ^ world_z) >> 8) & 1;
  } else {
    terrain_height += kNonGrassHeightOffset;
  }

  // Wall orientation and start point (1F9E3..1FAD0).
  int16_t wall_start_x = 0;
  int16_t wall_start_z = 0;
  int16_t wall_orientation = 0;
  if (wallindex >= 0 && static_cast<size_t>(wallindex) < walls_.size()) {
    const TrackWall& wall = walls_[wallindex];
    wall_orientation = static_cast<int16_t>(
        (wrap_negate(wall.orientation) + elem_orient + wall_orient_mod) & 0x300);
    switch (elem_orient) {
      case kOrient0:
        wall_start_x = wall.x;
        wall_start_z = wall.z;
        break;
      case kOrient90:
        wall_start_x = wall.z;
        wall_start_z = wrap_negate(wall.x);
        break;
      case kOrient180:
        wall_start_x = wrap_negate(wall.x);
        wall_start_z = wrap_negate(wall.z);
        break;
      case kOrient270:
        wall_start_x = wrap_negate(wall.z);
        wall_start_z = wall.x;
        break;
      default:
        break;
    }
    wall_start_x = wrap_add(wall_start_x, elem_x_center);
    wall_start_z = wrap_add(wall_start_z, elem_z_center);
  } else {
    wallindex = -1;
  }

  sample.plane_index = static_cast<int16_t>(final_plane);
  sample.terrain_height = static_cast<int16_t>(terrain_height);
  sample.surface_type = surface_type;
  sample.elem_x_center = elem_x_center;
  sample.elem_z_center = elem_z_center;
  sample.wall_index = static_cast<int16_t>(wallindex);
  sample.wall_start_x = wall_start_x;
  sample.wall_start_z = wall_start_z;
  sample.wall_orientation = wall_orientation;
  sample.wall_height = static_cast<int16_t>(wall_height);
  sample.wall_lower_bound = static_cast<int16_t>(wall_lower_bound);
  sample.physical_model = phys;
  *out = sample;
}

void TrackCollision::build_track_object(const fixed::Vec3& world_pos,
                                        const fixed::Vec3& next_world_pos,
                                        CollisionSample* out) const {
  if (track_ == nullptr) {
    if (out != nullptr) {
      CollisionSample sample;
      sample.plane_index = 0;
      sample.surface_type = static_cast<int8_t>(CarSurfaceType::Grass);
      *out = sample;
    }
    return;
  }
  build_track_object_from_track(*track_, world_pos, next_world_pos, out);
}

}  // namespace s3d::physics
