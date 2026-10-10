#include <string>

#include "assets/shape3d.h"
#include "assets/trk.h"
#include "core/paths.h"
#include "tiny_test.h"
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
