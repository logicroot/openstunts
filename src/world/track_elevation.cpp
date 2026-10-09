#include "world/track_elevation.h"

#include <algorithm>
#include <cmath>

namespace s3d::world {

namespace {
inline constexpr float kTileSize = 1024.0f;
inline constexpr int kTrackSize = 30;

// Snaps a world coordinate onto a grid cell index.
int world_to_cell(float w) {
  const int c = static_cast<int>(std::floor(w / kTileSize));
  return std::clamp(c, 0, kTrackSize - 1);
}
}  // namespace

GroundSample ground_sample_at(const Track& track, float wx, float wz) {
  const int x = world_to_cell(wx);
  const int y = world_to_cell(wz);

  GroundSample sample;
  sample.height = cell_ground_height(track, x, y);

  const uint8_t item = track.item(x, y);
  const uint8_t terrain = track.terrain_at(x, y);

  if (cell_is_drivable(track, x, y)) {
    // Elements are lifted one unit above the terrain so their surfaces do not
    // z-fight (track_shapes.cpp places elements at height + 1).
    sample.height += 1;
    items::Surface surf = items::surface_of(item);
    if (item >= 1 && item <= 3) surf = items::Surface::Road;
    sample.surface = surface_to_car_surface(surf);
  } else if (terrain_is_water(terrain)) {
    sample.surface = kCarSurfaceWater;
  } else {
    sample.surface = kCarSurfaceGrass;
  }
  return sample;
}

}  // namespace s3d::world