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
