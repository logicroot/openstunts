#pragma once
// Physical surface model shared by the renderer and the driving physics.
//
// The physics must ride on the *visible* rendered surface, so the height and
// surface rules here mirror `build_track_shape_geometry()` in track_shapes.cpp:
//   * terrain byte 6 (elevated plateau) draws at kElevatedHeight = 450, every
//     other terrain byte at y = 0;
//   * drivable elements (road/deck/line items) are placed 1 unit above the
//     terrain to avoid z-fighting, so a drivable cell samples height+1.
//
// Surface ids follow the original engine's CAR_SURFACE_* values (1..5); item
// bytes 1..3 (start/finish line and direction arrows) ride as paved road.

#include <cstdint>

#include "assets/trk.h"
#include "world/track_items.h"

namespace s3d::world {

// Car-surface ids, kept in sync with physics/car_state.h kCarSurface*.
inline constexpr int kCarSurfacePaved = 1;
inline constexpr int kCarSurfaceDirt = 2;
inline constexpr int kCarSurfaceIce = 3;
inline constexpr int kCarSurfaceGrass = 4;
inline constexpr int kCarSurfaceWater = 5;

// One sample of the ground under a wheel. `height` is in world units (the same
// units as the physics "word" positions); `surface` is a kCarSurface* id.
struct GroundSample {
  int height = 0;
  int surface = kCarSurfaceGrass;
};

// Maps an items::Surface to a car-surface id. Non-road surfaces fall back to
// grass (the reduced model treats anything non-drivable as soft ground).
inline int surface_to_car_surface(items::Surface s) {
  switch (s) {
    case items::Surface::Road:
    case items::Surface::Raised:
    case items::Surface::Bridge:
      return kCarSurfacePaved;
    case items::Surface::Dirt:
      return kCarSurfaceDirt;
    case items::Surface::Ice:
      return kCarSurfaceIce;
    default:
      return kCarSurfaceGrass;
  }
}

// Height (in world units) of the ground at grid cell (x, y). Mirrors the
// element placement in track_shapes.cpp (kElevatedHeight = 450).
// Height (in world units) of the ground at world position. Mirrors the
// element placement in track_shapes.cpp (kElevatedHeight = 450).
inline int cell_ground_height_at(const Track& track, int wx, int wz) {
  const int x = wx / 1024;
  const int y = wz / 1024;
  const int cx = x;
  const int cy = y;
  const uint8_t terr = track.terrain_at(cx, cy);
  if (terr == 6) return 450;
  return 0;
}

inline int cell_ground_height(const Track& track, int x, int y) {
  return track.terrain_at(x, y) == 6 ? 450 : 0;
}

// Whether cell (x, y) carries a drivable element. Items 1..3 (start/finish
// line and arrows) count as paved road so the car can cross the line.
inline bool cell_is_drivable(const Track& track, int x, int y) {
  const uint8_t item = track.item(x, y);
  if (item >= 1 && item <= 3) return true;  // start/finish and direction marks
  return items::is_drivable(item);
}

// Full ground sample for a point in world units. Cell boundaries are resolved
// with a simple clamp so off-track positions stay well defined.
GroundSample ground_sample_at(const Track& track, float wx, float wz);

// Water terrain (bytes 1..5) counts as a water surface for everything that is
// not a drivable element.
inline bool terrain_is_water(uint8_t terrain) { return terrain >= 1 && terrain <= 5; }

}  // namespace s3d::world