#include <cstring>
#include <string>

#include "assets/trk.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/track_objects.h"

using namespace s3d;
using namespace s3d::world;

TEST(track_object_table_matches_original) {
  EXPECT_EQ(track_object(0).shape, 30);       // empty slot still names a road shape
  EXPECT_EQ(track_object(4).shape, 30);       // vertical road
  EXPECT_EQ(track_object(4).lo_shape, 31);
  EXPECT_EQ(track_object(4).multi_tile, 0);
  EXPECT_EQ(track_object(5).rot_y, 256);      // horizontal road
  EXPECT_EQ(track_object(10).rot_y, 768);
  EXPECT_EQ(track_object(10).multi_tile, 3);  // 4-tile loop centre
  EXPECT_EQ(track_object(11).rot_y, 0);
  EXPECT_EQ(track_object(111).surface, 255);  // alternating surface
  EXPECT_EQ(track_object(182).shape, 28);     // hill-road ramp
  EXPECT_EQ(track_object(194).overlay, 222);
  EXPECT_EQ(track_object(201).multi_tile, 3);
  // Out-of-range access clamps into the table.
  EXPECT_EQ(track_object(-5).shape, track_object(0).shape);
  EXPECT_EQ(track_object(9999).shape, track_object(kTrackObjectCount - 1).shape);
}

TEST(terrain_shape_table_matches_original) {
  const TerrainShape* t = terrain_shapes();
  EXPECT_EQ(t[0].shape, 108);  // cfen (unused; terrain 0 draws a ground quad)
  EXPECT_EQ(t[1].shape, 45);   // lake
  EXPECT_EQ(t[2].shape, 44);   // lakc
  EXPECT_EQ(t[6].shape, 43);   // high (elevated flat)
  EXPECT_EQ(t[7].shape, 42);   // goup
  EXPECT_EQ(t[11].shape, 41);  // gouo
  EXPECT_EQ(t[15].shape, 40);  // goui
  EXPECT_EQ(t[3].rot_y, 768);
  EXPECT_EQ(t[4].rot_y, 512);
  EXPECT_EQ(t[5].rot_y, 256);
  EXPECT_EQ(t[8].rot_y, 768);
}

TEST(shape_names_follow_game_order) {
  EXPECT_EQ(std::strcmp(shape_name(0), "barn"), 0);
  EXPECT_EQ(std::strcmp(shape_name(30), "road"), 0);
  EXPECT_EQ(std::strcmp(shape_name(31), "zroa"), 0);
  EXPECT_EQ(std::strcmp(shape_name(40), "goui"), 0);
  EXPECT_EQ(std::strcmp(shape_name(43), "high"), 0);
  EXPECT_EQ(std::strcmp(shape_name(45), "lake"), 0);
  EXPECT_EQ(std::strcmp(shape_name(108), "cfen"), 0);
  EXPECT_EQ(std::strcmp(shape_name(124), ""), 0);  // car slot: outside the bank
}

TEST(track_object_at_resolves_continuations) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));
  PlacedTrackObject p;

  // multi_tile=2 element at (12,22): (13,22) is continuation 0xFF.
  EXPECT_TRUE(track_object_at(track, 12, 22, &p));
  EXPECT_EQ(p.item, 86);
  EXPECT_EQ(p.object->multi_tile, 2);
  EXPECT_TRUE(track_object_at(track, 13, 22, &p));
  EXPECT_EQ(p.item, 86);
  EXPECT_EQ(p.owner_x, 12);
  EXPECT_EQ(p.owner_y, 22);

  // multi_tile=1 element at (22,10): (22,9) is continuation 0xFE.
  EXPECT_TRUE(track_object_at(track, 22, 9, &p));
  EXPECT_EQ(p.item, 64);
  EXPECT_EQ(p.owner_x, 22);
  EXPECT_EQ(p.owner_y, 10);

  // multi_tile=3 element at (1,1): (2,0) is continuation 0xFD.
  EXPECT_TRUE(track_object_at(track, 2, 0, &p));
  EXPECT_EQ(p.item, 12);
  EXPECT_EQ(p.owner_x, 1);
  EXPECT_EQ(p.owner_y, 1);

  // Empty cell reports nothing.
  EXPECT_TRUE(!track_object_at(track, 29, 29, &p));
}
