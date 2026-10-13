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

// The checkpoint/arrow walk (seg001.asm sub_18D60): every route node's
// TRKOBJINFO block yields a short run of points that curve through the element,
// offset from the cell centre and rotated by the block's si_arrowOrient.
TEST(track_route_checkpoint_walks_the_loop) {
  const Track track = make_loop_track();
  TrackRoute route;
  EXPECT_TRUE(route.build(track, 3, 2, 0x000));

  RouteCheckpoint cp;

  // Start/finish straight (element 1): two points straddling the cell centre,
  // stepping north. The run returns false until its final point.
  EXPECT_TRUE(!route.checkpoint(0, 0, track, &cp));
  EXPECT_EQ(cp.x, 3584);
  EXPECT_EQ(cp.y, -1);
  EXPECT_EQ(cp.z, 28288);
  EXPECT_EQ(cp.ax, 3464);
  EXPECT_EQ(cp.bx, 3704);
  EXPECT_EQ(cp.opp1, 0);
  EXPECT_TRUE(route.checkpoint(0, 1, track, &cp));
  EXPECT_EQ(cp.z, 28494);

  // The west straight (element 5) runs east -> west: 2894 then 2226, on the
  // cell-centre z.
  EXPECT_TRUE(!route.checkpoint(2, 0, track, &cp));
  EXPECT_EQ(cp.x, 2894);
  EXPECT_EQ(cp.z, 29184);
  EXPECT_TRUE(route.checkpoint(2, 1, track, &cp));
  EXPECT_EQ(cp.x, 2226);

  // Out-of-range or invalid requests are rejected.
  EXPECT_TRUE(!route.checkpoint(0, 2, track, &cp));           // element 1 has 2 points
  EXPECT_TRUE(!route.checkpoint(0, -1, track, &cp));
  EXPECT_TRUE(!route.checkpoint(-1, 0, track, &cp));
  EXPECT_TRUE(!route.checkpoint(route.node_count(), 0, track, &cp));
}

// Regression over the shipped DEFAULT.TRK: the first checkpoint on the
// start/finish cell and the second beyond it.
TEST(track_route_checkpoint_default_track) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  const Track track = Track::load(path_join(dir, "DEFAULT.TRK"));
  const StartFinish sf = find_start_finish(track);
  TrackRoute route;
  EXPECT_TRUE(route.build(track, sf.col, Track::kSize - 1 - sf.row, sf.angle));

  RouteCheckpoint cp;
  EXPECT_TRUE(!route.checkpoint(0, 0, track, &cp));
  EXPECT_EQ(cp.x, 7680);   // start/finish cell centre
  EXPECT_EQ(cp.y, -1);
  EXPECT_EQ(cp.z, 5760);
  EXPECT_TRUE(route.checkpoint(0, 1, track, &cp));
  EXPECT_EQ(cp.z, 5966);
}

// seg004.asm load_opponent_data: the opponent course path (trackdata3).
TEST(track_route_opponent_path_walks_the_synthetic_loop) {
  const Track track = make_loop_track();
  TrackRoute route;
  EXPECT_TRUE(route.build(track, 3, 2, 0x000));

  // OPP1's sped table (only the cost weights use it; this loop has no forks).
  const uint8_t sped[16] = {120, 75, 50, 40, 30, 20, 60, 40,
                            20,  90, 115, 20, 30, 70, 80, 50};
  const std::vector<uint16_t> p = route.opponent_path(sped, 16);
  // The whole 8-node ring, then the original's 0, 0, 1 tail.
  const uint16_t want[11] = {0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 1};
  EXPECT_EQ(p.size(), 11u);
  for (size_t i = 0; i < p.size() && i < 11; ++i) EXPECT_EQ(p[i], want[i]);
}

// Over the shipped DEFAULT.TRK the path must be a real walk along the graph
// (successor or alternate at each step) that closes back onto node 0.
TEST(track_route_opponent_path_over_default_track) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  const Track track = Track::load(path_join(dir, "DEFAULT.TRK"));
  const StartFinish sf = find_start_finish(track);
  TrackRoute route;
  EXPECT_TRUE(route.build(track, sf.col, Track::kSize - 1 - sf.row, sf.angle));

  const uint8_t sped[16] = {120, 75, 50, 40, 30, 20, 60, 40,
                            20,  90, 115, 20, 30, 70, 80, 50};
  const std::vector<uint16_t> p = route.opponent_path(sped, 16);
  EXPECT_TRUE(p.size() >= 4u);
  EXPECT_EQ(p[p.size() - 1], 1);
  EXPECT_EQ(p[p.size() - 2], 0);
  EXPECT_EQ(p[p.size() - 3], 0);

  const size_t nodes = p.size() - 3;  // entries before the 0/0/1 tail
  for (size_t i = 0; i < nodes; ++i) {
    EXPECT_TRUE(p[i] < static_cast<uint16_t>(route.node_count()));
  }
  for (size_t i = 0; i + 1 < nodes; ++i) {
    const int a = p[i];
    const int b = p[i + 1];
    EXPECT_TRUE(route.successor(a) == b || route.alternate(a) == b);
  }
  EXPECT_EQ(route.successor(p[nodes - 1]), 0);  // the lap closes
}
