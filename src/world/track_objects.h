#pragma once
// Track element and terrain shape tables.
//
// A `.TRK` item byte (0..214) directly indexes `track_objects()`. Special
// continuation bytes 0xFD/0xFE/0xFF mark filler cells belonging to a multi-tile
// element; see `track_object_at()` for resolving them.
//
// Terrain bytes (1..18) index `terrain_shapes()`; terrain byte 0 draws nothing.
//
// Reference: restunts-bb11 `headless_trackdata.c` (trkObjectList[215]) and
// `full_data.c` (terrain_scene_objects[19]); see docs/formats.md.

#include <cstdint>
#include <string>

#include "assets/trk.h"

namespace s3d::world {

// Item byte values that mark a cell covered by a multi-tile element.
inline constexpr uint8_t kTileContinuationSoutheast = 0xFD;
inline constexpr uint8_t kTileContinuationSouth = 0xFE;
inline constexpr uint8_t kTileContinuationEast = 0xFF;

struct TrackObjectDef {
  int16_t rot_y = 0;       // yaw in 1/1024 turn (0, 256, 512, 768)
  uint8_t shape = 0;       // full-detail shape index
  uint8_t lo_shape = 0;    // low-detail shape index
  uint8_t overlay = 0;     // 0 = none, else index into track_objects()
  uint8_t surface = 0;     // 0/1/2 road variant, or 0xFF = alternating
  uint8_t ignore_z_bias = 0;
  uint8_t multi_tile = 0;  // 0=one tile, 1=+row (z), 2=+column (x), 3=both
  uint8_t phys = 0;        // physical model id
};

inline constexpr int kTrackObjectCount = 215;

const TrackObjectDef* track_objects();
const TrackObjectDef& track_object(int index);  // clamps into range

struct TerrainShape {
  int16_t rot_y = 0;
  uint8_t shape = 0;
  uint8_t lo_shape = 0;
};
inline constexpr int kTerrainShapeCount = 19;
const TerrainShape* terrain_shapes();

// Returns the name of a shape index in the fixed load order (0..115), or "" if
// the index is outside the track/scenery bank (cars, explosions).
const char* shape_name(int index);

// A resolved placed element. Continuation cells (0xFD/0xFE/0xFF) point back at
// the multi-tile owner cell that actually stores the item byte.
struct PlacedTrackObject {
  const TrackObjectDef* object = nullptr;
  int item = 0;      // resolved item code (owner's byte)
  int owner_x = 0;   // cell holding the real item byte
  int owner_y = 0;
};

// Resolves the element covering cell (x, y). Returns false for empty cells and
// dangling continuation markers.
bool track_object_at(const Track& track, int x, int y, PlacedTrackObject* out);

}  // namespace s3d::world
