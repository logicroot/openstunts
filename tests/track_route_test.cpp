// Route graph tests: the seg004.asm `track_setup` walker (world/track_route)
// over the synthetic ring from synthetic_track.h, the error-code names, and a
// regression check over the shipped DEFAULT.TRK.

#include <string>

#include "assets/trk.h"
#include "core/paths.h"
#include "game/race_state.h"
#include "synthetic_track.h"
#include "tiny_test.h"
#include "world/track_route.h"

using namespace s3d;
using namespace s3d::game;
using namespace s3d::world;
using namespace s3d_test;

TEST(track_route_builds_synthetic_loop) {
  const Track t = make_loop_track();
  TrackRoute route;
  // Start cell (col 3, bottom-up row 2), start/finish direction 0.
  EXPECT_TRUE(route.build(t, 3, 2, 0x000));
  EXPECT_TRUE(route.valid());
  EXPECT_EQ(route.node_count(), 8);
  EXPECT_EQ(route.start_col(), 3);
  EXPECT_EQ(route.start_row(), 2);

  // Node i sits on the ring cell and steps to node i+1 (wrap to 0); no forks.
  const int cells[8][2] = {{3, 2}, {3, 1}, {2, 1}, {1, 1}, {1, 2}, {1, 3}, {2, 3}, {3, 3}};
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(route.node(i).col, cells[i][0]);
    EXPECT_EQ(route.node(i).row, cells[i][1]);
    EXPECT_EQ(route.successor(i), (i + 1) % 8);
    EXPECT_EQ(route.alternate(i), -1);
  }
  EXPECT_EQ(route.node(0).elem, 0x01);  // start/finish straight
  EXPECT_EQ(route.node(1).elem, 0x07);  // north -> west corner
  EXPECT_EQ(route.node(3).elem, 0x06);  // west -> south corner
  EXPECT_EQ(route.node(7).elem, 0x09);  // east -> north corner
}

TEST(track_route_reports_error_name) {
  EXPECT_EQ(std::string(route_error_name(RouteError::none)), "none");
  EXPECT_EQ(std::string(route_error_name(RouteError::no_runway)), "no_runway");
  EXPECT_EQ(std::string(route_error_name(RouteError::wrong_way)), "wrong_way");
}

TEST(track_route_empty_track_fails) {
  Track t;
  TrackRoute route;
  // No elements: the walk cannot leave the start cell.
  EXPECT_TRUE(!route.build(t, 3, 2, 0x000));
  EXPECT_TRUE(!route.valid());
}

TEST(track_route_default_track_matches_reference) {
  // Regression check against the values produced by the reference walker over
  // the shipped DEFAULT.TRK (see the port notes): 128 nodes from the start
  // cell (col 7, bottom-up row 24), a closed 59-node main loop 0..58, and a
  // ring that every node can leave (no leaf nodes).
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  const Track track = Track::load(path_join(dir, "DEFAULT.TRK"));
  const StartFinish sf = find_start_finish(track);
  EXPECT_TRUE(sf.found);

  TrackRoute route;
  EXPECT_TRUE(route.build(track, sf.col, Track::kSize - 1 - sf.row, sf.angle));
  EXPECT_TRUE(route.valid());
  EXPECT_EQ(route.node_count(), 128);
  EXPECT_EQ(route.start_col(), 7);
  EXPECT_EQ(route.start_row(), 24);

  // Every node has a successor, so detect_penalty never walks off the graph.
  for (int i = 0; i < route.node_count(); ++i) {
    EXPECT_TRUE(route.successor(i) >= 0);
    EXPECT_TRUE(route.successor(i) < route.node_count());
  }

  // The main loop closes at node 58 -> 0.
  int node = 0;
  int steps = 0;
  while (steps < 200) {
    node = route.successor(node);
    ++steps;
    if (node == 0) break;
  }
  EXPECT_EQ(node, 0);
  EXPECT_EQ(steps, 59);

  // A few landmark nodes (col, row) / element from the reference walk.
  EXPECT_EQ(route.node(0).elem, 0x01);   // start/finish straight
  EXPECT_EQ(route.node(10).col, 6);      // 2x2 multi-tile piece
  EXPECT_EQ(route.node(10).row, 13);
  EXPECT_EQ(route.node(10).elem, 87);
  EXPECT_EQ(route.node(43).col, 7);      // the overpass crossing node
  EXPECT_EQ(route.node(43).row, 23);
  EXPECT_EQ(route.node(127).row, 26);
  EXPECT_TRUE(route.alternate(9) >= 0);  // node 9 forks onto the spur
}
