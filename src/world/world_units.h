#pragma once
// World-space scale constants shared by the whole game.
//
// Stunts works in a fixed-point world; exact unit values are being confirmed
// against the restunts reference. Everything here is expressed in the game's
// native units so that asset data can be used unscaled.

namespace s3d::world {

// One track grid square is 1024 world units in the original engine.
inline constexpr float kSquareSize = 1024.0f;

// Height of one terrain elevation step ("hill level"). Provisional; refined
// once the terrain-height tables are confirmed.
inline constexpr float kHillHeight = 256.0f;

// Water surface relative to sea level.
inline constexpr float kWaterLevel = -64.0f;

inline constexpr int kTrackSize = 30;

// World position of the center of grid cell (x, y) at height h.
inline float cell_center_x(int x) { return (static_cast<float>(x) + 0.5f) * kSquareSize; }
inline float cell_center_z(int y) { return (static_cast<float>(y) + 0.5f) * kSquareSize; }

}  // namespace s3d::world
