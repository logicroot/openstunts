#pragma once
// Builds placeholder 3D geometry from a .TRK track: terrain heightfield plus
// drivable-surface slabs and markers for special elements. This is the M1
// visualization; real element meshes (from GAME1/GAME2.P3S) replace the slabs
// in a later milestone.

#include "assets/trk.h"
#include "render/mesh.h"

namespace s3d {

struct TrackMeshStats {
  int terrain_quads = 0;
  int road_quads = 0;
  int special_cells = 0;
  int water_cells = 0;
};

Mesh build_track_mesh(const Track& track, TrackMeshStats* stats = nullptr);

}  // namespace s3d
