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
  int elements = 0;      // placed objects that never change appearance
  int overlays = 0;
  int animated = 0;      // placed objects whose paint cycle (surface == 0xFF)
  int missing_shapes = 0;
};

// Number of distinct paint slots an animated object cycles through per loop.
inline constexpr int kTrackAnimationPhases = 4;

// Paint slot for animated objects at a given game frame. Mirrors the original
// `byte_3C0C6[game_frame & 15]` = {0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3}: a 0..3
// phase that advances every four frames. Objects with `surface == 0xFF`
// (windmill sails, Joe's flashing sign, ...) use this as their material slot.
int track_animation_phase(int game_frame);

// Builds CPU-side geometry for `track` into `out`. `lib` must contain the GAME1
// (elements) and GAME2 (scenery/terrain) shape archives. This never touches the
// GPU, so it is safe to call in headless tests. Colours come from the built-in
// fallback table (world/shape_mesh.h track_paint_color). Animated objects are
// built at phase 0.
void build_track_shape_geometry(const Track& track, const ShapeLibrary& lib,
                                MeshBuilder& out, TrackShapeStats* stats = nullptr);

// As above, but each paint index is resolved through the real material table
// (world/material.h) and sampled from the VGA `palette`, matching the original
// game colours.
void build_track_shape_geometry(const Track& track, const ShapeLibrary& lib,
                                MeshBuilder& out, const Palette& palette,
                                TrackShapeStats* stats = nullptr);

// Static subset: terrain plus every element that never changes appearance.
// Animated objects (surface == 0xFF) are omitted and drawn from
// build_track_animated_geometry() instead. `palette` colours as above.
void build_track_static_geometry(const Track& track, const ShapeLibrary& lib,
                                 MeshBuilder& out, const Palette& palette,
                                 TrackShapeStats* stats = nullptr);
void build_track_static_geometry(const Track& track, const ShapeLibrary& lib,
                                 MeshBuilder& out, TrackShapeStats* stats = nullptr);

// Animated subset for one `phase` (0 .. kTrackAnimationPhases-1): the windmill
// sails, the flashing diner sign and similar objects. Callers rebuild/select
// this as the phase advances. Line primitives (the diner sign) are appended to
// `lines` when it is non-null, for drawing as GL_LINES.
void build_track_animated_geometry(const Track& track, const ShapeLibrary& lib,
                                   MeshBuilder& out, const Palette& palette, int phase,
                                   TrackShapeStats* stats = nullptr,
                                   MeshBuilder* lines = nullptr);
void build_track_animated_geometry(const Track& track, const ShapeLibrary& lib,
                                   MeshBuilder& out, int phase,
                                   TrackShapeStats* stats = nullptr,
                                   MeshBuilder* lines = nullptr);

// ---- Layered output ---------------------------------------------------------
// The track is a stack of flat, nearly coplanar layers: the ground/terrain base
// at the bottom, then terrain relief, then placed elements (roads, bridges),
// then the decals an element carries. Drawn through a depth buffer they
// z-fight wherever the layers are within the depth resolution of each other,
// which flickers when the camera moves. Splitting them into separate meshes
// lets the renderer push each layer behind the next with glPolygonOffset so the
// draw order is authoritative. A null pointer for a layer folds that layer's
// geometry into `surface` (so a single-buffer call reproduces the old order).
struct TrackMeshLayers {
  MeshBuilder* base = nullptr;     // ground quads and terrain bases (lowest)
  MeshBuilder* terrain = nullptr;  // terrain relief shapes
  MeshBuilder* surface = nullptr;  // placed elements (roads, bridges, ...) [required]
  MeshBuilder* overlay = nullptr;  // element decals (drawn on top of surface)
  MeshBuilder* lines = nullptr;    // GL_LINES primitives (animated sign only)
};

void build_track_static_layers(const Track& track, const ShapeLibrary& lib,
                               const TrackMeshLayers& layers, const Palette& palette,
                               TrackShapeStats* stats = nullptr);
void build_track_static_layers(const Track& track, const ShapeLibrary& lib,
                               const TrackMeshLayers& layers,
                               TrackShapeStats* stats = nullptr);

void build_track_animated_layers(const Track& track, const ShapeLibrary& lib,
                                 const TrackMeshLayers& layers, const Palette& palette,
                                 int phase, TrackShapeStats* stats = nullptr);
void build_track_animated_layers(const Track& track, const ShapeLibrary& lib,
                                 const TrackMeshLayers& layers, int phase,
                                 TrackShapeStats* stats = nullptr);

// Convenience wrappers that upload the geometry to a GPU mesh.
Mesh build_track_shape_mesh(const Track& track, const ShapeLibrary& lib,
                            TrackShapeStats* stats = nullptr);
Mesh build_track_shape_mesh(const Track& track, const ShapeLibrary& lib,
                            const Palette& palette, TrackShapeStats* stats = nullptr);

}  // namespace s3d::world
