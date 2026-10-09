#include "world/track_shapes.h"

#include <glm/glm.hpp>

#include <cmath>
#include <cstdint>

#include "physics/fixed_math.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"
#include "world/world_units.h"

namespace s3d::world {

namespace {

constexpr float kElevatedHeight = 450.0f;  // hillHeightConsts[1]
constexpr float kTileSize = 1024.0f;
constexpr float kHalf = 512.0f;

// ---- Transforms ------------------------------------------------------------

// Builds a translation + fixed-point yaw (1/1024 turn) matrix matching the
// original `mat_rot_y` (track elements store their yaw in `rotvec.z`, which
// `mat_rot_zxy` applies as the Y rotation).
glm::mat4 fixed_model(const glm::vec3& t, int16_t rot_y) {
  fixed::Mat3 r;
  fixed::mat_rot_y(rot_y, &r);
  glm::mat4 m(1.0f);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      m[col][row] = static_cast<float>(r.at(row, col)) / 16384.0f;
    }
  }
  m[3][0] = t.x;
  m[3][1] = t.y;
  m[3][2] = t.z;
  return m;
}

const Shape3D* lookup(const ShapeLibrary& lib, int index) {
  if (index < 0) return nullptr;
  const char* name = shape_name(index);
  if (name == nullptr || name[0] == '\0') return nullptr;
  return lib.find(name);
}

// Resolves an overlay index to (shape, yaw). Indices >= 215 fall through into
// the terrain scene table (restunts `frame_track_object_from_legacy_index`).
bool overlay_shape(int index, uint8_t* shape, int16_t* rot_y) {
  if (index <= 0) return false;
  if (index < kTrackObjectCount) {
    const TrackObjectDef& o = track_object(index);
    *shape = o.shape;
    *rot_y = o.rot_y;
    return true;
  }
  const int terrain_index = index - kTrackObjectCount;
  if (terrain_index >= 0 && terrain_index < kTerrainShapeCount) {
    const TerrainShape& ts = terrain_shapes()[terrain_index];
    *shape = ts.shape;
    *rot_y = ts.rot_y;
    return true;
  }
  return false;
}

// Hill terrain 7..10 replaces a flat road piece with the sloped variant.
// Returns the substitute item code, or 0 if `item` is not a hill-road piece.
int subst_hillroad(int terrain, int item) {
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
  if (terrain < 7 || terrain > 10) return 0;
  const int row = terrain - 7;
  for (int p = 0; p < 6; ++p) {
    if (kPieces[row][p] == item) {
      return kShapes[row][p < 3 ? p : 3];
    }
  }
  return 0;
}

void add_ground_quad(MeshBuilder& mb, float cx, float cz, const glm::vec4& color) {
  const glm::vec3 a(cx - kHalf, 0.0f, cz - kHalf);
  const glm::vec3 b(cx + kHalf, 0.0f, cz + kHalf);
  const glm::vec3 c(cx + kHalf, 0.0f, cz - kHalf);
  const glm::vec3 d(cx - kHalf, 0.0f, cz + kHalf);
  mb.add_quad(a, b, c, d, color, glm::vec3(0.0f, 1.0f, 0.0f));
}

}  // namespace

Mesh build_track_shape_mesh(const Track& track, const ShapeLibrary& lib,
                            TrackShapeStats* stats) {
  MeshBuilder mb;
  build_track_shape_geometry(track, lib, mb, stats);
  return mb.to_mesh();
}

void build_track_shape_geometry(const Track& track, const ShapeLibrary& lib,
                                MeshBuilder& mb, TrackShapeStats* stats) {
  TrackShapeStats local;
  if (stats != nullptr) *stats = local;

  const glm::vec4 ground_color(0.33f, 0.55f, 0.26f, 1.0f);
  const TerrainShape* terrains = terrain_shapes();

  // ---- Terrain -------------------------------------------------------------
  for (int y = 0; y < Track::kSize; ++y) {
    for (int x = 0; x < Track::kSize; ++x) {
      const int t = track.terrain_at(x, y);
      const float cx = static_cast<float>(x) * kTileSize + kHalf;
      const float cz = static_cast<float>(y) * kTileSize + kHalf;
      if (t == 0) {
        add_ground_quad(mb, cx, cz, ground_color);
        ++local.ground_cells;
      } else if (t > 0 && t < kTerrainShapeCount) {
        const TerrainShape& ts = terrains[t];
        const Shape3D* s = lookup(lib, ts.shape);
        const float height = (t == 6) ? kElevatedHeight : 0.0f;
        if (s != nullptr) {
          append_shape(mb, *s, fixed_model({cx, height, cz}, ts.rot_y), track_paint_color);
          ++local.terrain_shapes;
        } else {
          ++local.missing_shapes;
        }
      }
    }
  }

  // ---- Track elements ------------------------------------------------------
  for (int y = 0; y < Track::kSize; ++y) {
    for (int x = 0; x < Track::kSize; ++x) {
      int item = track.item(x, y);
      if (item == 0 || item >= kTileContinuationSoutheast) continue;  // empty/filler

      const int terrain = track.terrain_at(x, y);
      const int sub = subst_hillroad(terrain, item);
      if (sub != 0) item = sub;

      const TrackObjectDef& def = track_object(item);
      const float height = (terrain == 6) ? kElevatedHeight : 0.0f;
      const float px = (def.multi_tile & 2) ? (x + 1) * kTileSize : x * kTileSize + kHalf;
      const float pz = (def.multi_tile & 1) ? y * kTileSize : y * kTileSize + kHalf;

      // Lift slightly so element surfaces do not z-fight with the terrain.
      const glm::vec3 pos(px, height + 1.0f, pz);

      const Shape3D* s = lookup(lib, def.shape);
      if (s != nullptr) {
        append_shape(mb, *s, fixed_model(pos, def.rot_y), track_paint_color);
        ++local.elements;
      } else {
        ++local.missing_shapes;
      }

      if (def.overlay != 0) {
        uint8_t oshape = 0;
        int16_t orot = 0;
        if (overlay_shape(def.overlay, &oshape, &orot)) {
          const Shape3D* os = lookup(lib, oshape);
          if (os != nullptr) {
            append_shape(mb, *os, fixed_model(pos, orot), track_paint_color);
            ++local.overlays;
          } else {
            ++local.missing_shapes;
          }
        }
      }
    }
  }

  if (stats != nullptr) *stats = local;
}

}  // namespace s3d::world
