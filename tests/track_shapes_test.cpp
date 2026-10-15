#include <string>

#include "assets/palette.h"
#include "assets/shape3d.h"
#include "assets/trk.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/material.h"
#include "world/mesh_builder.h"
#include "world/track_shapes.h"

using namespace s3d;
using namespace s3d::world;

TEST(build_track_shape_geometry_default_track) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }

  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  if (lib.shapes().empty()) {
    std::printf("  (skipped: no P3S shapes found)\n");
    return;
  }

  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));
  MeshBuilder mb;
  TrackShapeStats stats;
  build_track_shape_geometry(track, lib, mb, &stats);

  EXPECT_TRUE(mb.vertex_count() > 0);
  EXPECT_TRUE(mb.indices().size() % 3 == 0);
  EXPECT_EQ(stats.ground_cells, 826);   // 900 - 74 non-flat terrain cells
  EXPECT_EQ(stats.missing_shapes, 0);
  EXPECT_TRUE(stats.elements > 100);
  EXPECT_TRUE(stats.terrain_shapes > 0);
  EXPECT_TRUE(stats.overlays > 0);
}

// Elevated and sloped road surfaces are stippled by the original
// (draw_patterned_lines, seg012.asm); build_track_shape_geometry must carry
// those material dither masks into the vertex stream so the renderer can
// reproduce them, while flat materials stay flat (pattern 0).
TEST(build_track_shape_geometry_carries_pattern_masks) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  if (lib.shapes().empty()) return;
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));

  MeshBuilder mb;
  build_track_shape_geometry(track, lib, mb);
  EXPECT_TRUE(mb.vertex_count() > 0);

  bool saw_flat = false;
  bool saw_pattern = false;
  bool saw_deck_mask = false;
  for (const Vertex& v : mb.vertices()) {
    if (v.pattern == 0.0f) {
      saw_flat = true;
    } else {
      saw_pattern = true;
      // Materials 22/23/24 (elevated/sloped road deck and its dashes) all use
      // the 0x77DD tile.
      if (v.pattern == 30685.0f) saw_deck_mask = true;
    }
  }
  EXPECT_TRUE(saw_flat);       // flat roads, ground and scenery
  EXPECT_TRUE(saw_pattern);    // DEFAULT.TRK has elevated/sloped road pieces
  EXPECT_TRUE(saw_deck_mask);  // 0x77DD deck mask
}

TEST(build_track_shape_geometry_is_deterministic) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  if (lib.shapes().empty()) return;
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));

  MeshBuilder a;
  MeshBuilder b;
  build_track_shape_geometry(track, lib, a);
  build_track_shape_geometry(track, lib, b);
  EXPECT_EQ(a.vertex_count(), b.vertex_count());
  EXPECT_EQ(a.indices().size(), b.indices().size());
}

TEST(build_track_shape_geometry_uses_palette_colors) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  if (lib.shapes().empty()) return;
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));

  // Fill every palette entry with one distinctive colour: if the builder routes
  // paint indices through the material table + palette, every vertex (including
  // the flat ground quad) ends up that colour. The fallback path would not.
  Palette pal;
  for (int i = 0; i < 256; ++i) {
    pal.rgb8[i][0] = 51;
    pal.rgb8[i][1] = 102;
    pal.rgb8[i][2] = 153;
  }
  const glm::vec4 expected(51.0f / 255.0f, 102.0f / 255.0f, 153.0f / 255.0f, 1.0f);

  MeshBuilder mb;
  build_track_shape_geometry(track, lib, mb, pal);
  EXPECT_TRUE(mb.vertex_count() > 0);
  bool all_match = true;
  for (const Vertex& v : mb.vertices()) {
    if (v.color != expected) all_match = false;
  }
  EXPECT_TRUE(all_match);
}

// A road's markings are authored coincident with the road surface, so through a
// depth buffer they z-fight and the line flickers as the camera moves. When the
// caller supplies a `decal` layer, build_track_static_layers must move those
// markings out of `surface`/`overlay` and into `decal` (which the renderer draws
// with the strongest forward polygon offset).
TEST(build_track_static_layers_routes_markings_to_decal) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  if (lib.shapes().empty()) return;
  Palette pal;
  if (!load_vga_palette(path_join(dir, "SDMAIN.PVS"), &pal)) {
    std::printf("  (skipped: no palette)\n");
    return;
  }
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));

  MeshBuilder base, terrain, surface, overlay, decal;
  TrackMeshLayers layers;
  layers.base = &base;
  layers.terrain = &terrain;
  layers.surface = &surface;
  layers.overlay = &overlay;
  layers.decal = &decal;
  build_track_static_layers(track, lib, layers, pal);

  EXPECT_TRUE(decal.vertex_count() > 0);

  // Paint 21 is the yellow road line; it must appear only in the decal layer.
  const glm::vec4 road_line = material_color(21, pal);
  bool surface_has_line = false;
  bool overlay_has_line = false;
  bool decal_has_line = false;
  for (const Vertex& v : surface.vertices()) {
    if (v.color == road_line) surface_has_line = true;
  }
  for (const Vertex& v : overlay.vertices()) {
    if (v.color == road_line) overlay_has_line = true;
  }
  for (const Vertex& v : decal.vertices()) {
    if (v.color == road_line) decal_has_line = true;
  }
  EXPECT_TRUE(decal_has_line);
  EXPECT_TRUE(!surface_has_line);
  EXPECT_TRUE(!overlay_has_line);

  // Splitting into layers must not drop or duplicate any geometry.
  MeshBuilder whole;
  build_track_static_geometry(track, lib, whole, pal);
  const size_t split = base.vertex_count() + terrain.vertex_count() + surface.vertex_count() +
                       overlay.vertex_count() + decal.vertex_count();
  EXPECT_EQ(split, whole.vertex_count());
}

// The phase table is byte_3C0C6 = {0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3}, repeated.
TEST(track_animation_phase_matches_reference) {
  const int expected[16] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
  for (int f = 0; f < 16; ++f) EXPECT_EQ(track_animation_phase(f), expected[f]);
  for (int f = 0; f < 64; ++f) {
    EXPECT_EQ(track_animation_phase(f), track_animation_phase(f + 16));
  }
}

// Splitting the build into a static mesh plus the phase-0 animated mesh must
// reproduce the full one-shot build exactly (same primitives, phase 0).
TEST(build_track_static_plus_animated_matches_full) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  if (lib.shapes().empty()) return;
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));

  Palette pal;
  for (int i = 0; i < 256; ++i) {
    pal.rgb8[i][0] = 51;
    pal.rgb8[i][1] = 102;
    pal.rgb8[i][2] = 153;
  }

  MeshBuilder full;
  MeshBuilder stat;
  MeshBuilder anim;
  build_track_shape_geometry(track, lib, full, pal);
  build_track_static_geometry(track, lib, stat, pal);
  build_track_animated_geometry(track, lib, anim, pal, 0);

  EXPECT_TRUE(anim.vertex_count() > 0);  // DEFAULT.TRK has windmill/diner objects
  EXPECT_EQ(stat.vertex_count() + anim.vertex_count(), full.vertex_count());
}

// Advancing the phase must change the animated geometry (the visible sail
// wedges move). Vertex counts stay equal because exactly one paint slot per
// blade triangle is opaque at every phase.
TEST(build_track_animated_geometry_changes_with_phase) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  if (lib.shapes().empty()) return;
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));

  Palette pal;
  for (int i = 0; i < 256; ++i) {
    pal.rgb8[i][0] = 51;
    pal.rgb8[i][1] = 102;
    pal.rgb8[i][2] = 153;
  }

  MeshBuilder a;
  MeshBuilder b;
  build_track_animated_geometry(track, lib, a, pal, 0);
  build_track_animated_geometry(track, lib, b, pal, 1);
  EXPECT_TRUE(a.vertex_count() > 0);
  EXPECT_EQ(a.vertex_count(), b.vertex_count());

  bool differs = false;
  for (size_t i = 0; i < a.vertices().size(); ++i) {
    if (a.vertices()[i].position != b.vertices()[i].position) differs = true;
  }
  EXPECT_TRUE(differs);
}

// The diner sign is made of line primitives; the animated builder collects them
// into the optional line output (for GL_LINES) instead of dropping them.
TEST(build_track_animated_geometry_collects_sign_lines) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return;
  ShapeLibrary lib;
  lib.load_p3s(path_join(dir, "GAME1.P3S"));
  lib.load_p3s(path_join(dir, "GAME2.P3S"));
  if (lib.shapes().empty()) return;
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));  // contains Joe's diner

  Palette pal;
  for (int i = 0; i < 256; ++i) {
    pal.rgb8[i][0] = 51;
    pal.rgb8[i][1] = 102;
    pal.rgb8[i][2] = 153;
  }

  MeshBuilder anim;
  MeshBuilder lines;
  build_track_animated_geometry(track, lib, anim, pal, 0, nullptr, &lines);
  EXPECT_TRUE(anim.vertex_count() > 0);
  EXPECT_TRUE(lines.vertex_count() > 0);
  EXPECT_TRUE(lines.vertex_count() % 2 == 0);  // pairs for GL_LINES
}
