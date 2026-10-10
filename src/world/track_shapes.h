#pragma once
// Builds the full track geometry (terrain + placed elements) out of the
// original `GAME1`/`GAME2` P3S shapes.
//
// Placement follows restunts `frame.c` / `trackobj.c`:
//   * terrain byte t (1..18) draws `terrain_shapes()[t]` at the cell center, at
//     y = 0 (or y = 450 for the elevated-flat terrain 6);
//   * terrain byte 0 is flat land at sea level and draws a plain ground quad;
//   * item byte i draws `track_object(i)` at the cell center (or the shared
//     edge for multi-tile elements), then its overlay object on top;
//   * hill terrain 7..10 substitutes special sloped "hill road" pieces.
//
// Reference: restunts-bb11 `frame.c` (frame_draw_terrain / frame_add_track_element),
// `trkutil.c` (subst_hillroad_track), `full_data.c` (terrain_scene_objects).

#include <string>

#include "assets/palette.h"
#include "assets/shape3d.h"
#include "assets/trk.h"
#include "render/mesh.h"
#include "world/mesh_builder.h"
#include "world/track_objects.h"

namespace s3d::world {

struct TrackShapeStats {
  int ground_cells = 0;
  int terrain_shapes = 0;
  int elements = 0;
  int overlays = 0;
  int missing_shapes = 0;
};

// Builds CPU-side geometry for `track` into `out`. `lib` must contain the GAME1
// (elements) and GAME2 (scenery/terrain) shape archives. This never touches the
// GPU, so it is safe to call in headless tests. Colours come from the built-in
// fallback table (world/shape_mesh.h track_paint_color).
void build_track_shape_geometry(const Track& track, const ShapeLibrary& lib,
                                MeshBuilder& out, TrackShapeStats* stats = nullptr);

// As above, but each paint index is resolved through the real material table
// (world/material.h) and sampled from the VGA `palette`, matching the original
// game colours.
void build_track_shape_geometry(const Track& track, const ShapeLibrary& lib,
                                MeshBuilder& out, const Palette& palette,
                                TrackShapeStats* stats = nullptr);

// Convenience wrappers that upload the geometry to a GPU mesh.
Mesh build_track_shape_mesh(const Track& track, const ShapeLibrary& lib,
                            TrackShapeStats* stats = nullptr);
Mesh build_track_shape_mesh(const Track& track, const ShapeLibrary& lib,
                            const Palette& palette, TrackShapeStats* stats = nullptr);

}  // namespace s3d::world
