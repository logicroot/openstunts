// Race loop tests: the start/finish element scan (seg004.asm), the finish-line
// signed-distance test (state.c) and the format_frame_as_string clock format.

#include <string>

#include "assets/trk.h"
#include "game/race_state.h"
#include "synthetic_track.h"
#include "tiny_test.h"

using namespace s3d;
using namespace s3d::game;
using namespace s3d_test;

namespace {

constexpr int kSquare = 1024;
int center(int index) { return index * kSquare + kSquare / 2; }

StartFinish make_sf(int col, int row, int angle) {
  StartFinish sf;
  sf.found = true;
  sf.col = col;
  sf.row = row;
  sf.angle = angle;
  return sf;
}

}  // namespace

TEST(race_start_finish_angle_table) {
  EXPECT_EQ(start_finish_angle(0x01), 0x000);
  EXPECT_EQ(start_finish_angle(0x86), 0x000);
  EXPECT_EQ(start_finish_angle(0x87), 0x200);
  EXPECT_EQ(start_finish_angle(0x88), 0x100);
  EXPECT_EQ(start_finish_angle(0x89), 0x300);
  EXPECT_EQ(start_finish_angle(0x93), 0x000);
  EXPECT_EQ(start_finish_angle(0x94), 0x200);
  EXPECT_EQ(start_finish_angle(0x95), 0x100);
  EXPECT_EQ(start_finish_angle(0x96), 0x300);
  EXPECT_EQ(start_finish_angle(0xB3), 0x200);
  EXPECT_EQ(start_finish_angle(0xB4), 0x100);
  EXPECT_EQ(start_finish_angle(0xB5), 0x300);
  EXPECT_EQ(start_finish_angle(0x04), -1);  // plain road
  EXPECT_EQ(start_finish_angle(0x00), -1);  // blank
}

TEST(race_find_start_finish_scans_grid) {
  Track t;
  t.items[static_cast<size_t>(5) * Track::kSize + 7] = 0x95;
  const StartFinish sf = find_start_finish(t);
  EXPECT_TRUE(sf.found);
  EXPECT_TRUE(!sf.multiple);
  EXPECT_EQ(sf.col, 7);
  EXPECT_EQ(sf.row, 5);
  EXPECT_EQ(sf.angle, 0x100);
}

TEST(race_find_start_finish_ignores_high_filler) {
  Track t;
  t.items[0] = 0xFD;  // filler -> blank
  t.items[1] = 0xB6;  // -> plain road (4)
  t.items[2] = 0x01;  // real start/finish
  const StartFinish sf = find_start_finish(t);
  EXPECT_TRUE(sf.found);
  EXPECT_TRUE(!sf.multiple);
  EXPECT_EQ(sf.col, 2);
  EXPECT_EQ(sf.row, 0);
}

TEST(race_find_start_finish_flags_multiple) {
  Track t;
  t.items[static_cast<size_t>(0) * Track::kSize + 2] = 0x01;
  t.items[static_cast<size_t>(3) * Track::kSize + 4] = 0x94;
  const StartFinish sf = find_start_finish(t);
  EXPECT_TRUE(sf.found);
  EXPECT_TRUE(sf.multiple);
  EXPECT_EQ(sf.col, 2);  // the first in row-major order is kept
}

TEST(race_finish_line_distance_signs) {
  // angle 0 points along +row (z). The car is behind when z < center.
  const StartFinish sf = make_sf(7, 5, 0);
  const int cz = center(5);
  EXPECT_TRUE(finish_line_distance(0, (cz - 210) * 64, sf) > 0);
  EXPECT_TRUE(finish_line_distance(0, (cz + 100) * 64, sf) < 0);

  // angle 0x100 points along +column (x).
  const StartFinish sf90 = make_sf(7, 5, 0x100);
  const int cx = center(7);
  EXPECT_TRUE(finish_line_distance((cx - 210) * 64, 0, sf90) > 0);
  EXPECT_TRUE(finish_line_distance((cx + 100) * 64, 0, sf90) < 0);

  // With no line the test must never report a crossing.
  StartFinish none;
  EXPECT_TRUE(finish_line_distance(0, 0, none) > 0);
}

TEST(race_format_matches_frame_as_string) {
  EXPECT_EQ(format_race_time(0, 20, false), std::string(" 0:00"));
  EXPECT_EQ(format_race_time(20, 20, false), std::string(" 0:01"));
  EXPECT_EQ(format_race_time(59 * 20, 20, false), std::string(" 0:59"));
  EXPECT_EQ(format_race_time(60 * 20, 20, false), std::string(" 1:00"));
  EXPECT_EQ(format_race_time(10 * 60 * 20, 20, false), std::string("10:00"));
  // frames 1234 = 1 min 1.7 s at 20 fps.
  EXPECT_EQ(format_race_time(1234, 20, true), std::string(" 1:01.70"));
  // 10 fps uses (100 / 10) = 10 per frame.
  EXPECT_EQ(format_race_time(60 * 10 + 3 * 10 + 4, 10, true), std::string(" 1:03.40"));
}

TEST(race_state_route_gate_and_finish) {
  const Track t = make_loop_track();
  const StartFinish sf = find_start_finish(t);
  EXPECT_TRUE(sf.found);
  EXPECT_EQ(sf.col, 3);
  EXPECT_EQ(sf.row, 27);

  RaceState rs;
  EXPECT_TRUE(rs.build_route(t, sf));
  EXPECT_EQ(rs.route.node_count(), 8);  // the 8-cell ring
  rs.tick();
  rs.tick();
  rs.tick();
  EXPECT_EQ(rs.game_frame, 3);
  rs.penalty = 40;

  // The car spawns on the start cell (route node 0): detect_penalty short
  // circuits, so field_CD stays 0.
  EXPECT_TRUE(!rs.update_finish(center(3) * 64, (center(27) - 210) * 64, sf));
  EXPECT_EQ(rs.field_cd, 0);
  EXPECT_TRUE(!rs.finished);

  // Crossing the line forward while still inside the start cell does not arm.
  EXPECT_TRUE(!rs.update_finish(center(3) * 64, (center(27) + 10) * 64, sf));
  EXPECT_EQ(rs.field_cd, 0);

  // Drive the ring cell by cell (bottom-up rows: node 1..7 at file rows
  // 28,28,28,27,26,26,26). Each step locates the next route node.
  const int path[7][2] = {{3, 28}, {2, 28}, {1, 28}, {1, 27}, {1, 26}, {2, 26}, {3, 26}};
  for (int i = 0; i < 7; ++i) {
    EXPECT_TRUE(!rs.update_finish(center(path[i][0]) * 64, center(path[i][1]) * 64, sf));
    EXPECT_EQ(rs.field_2f2, i + 1);
  }
  EXPECT_EQ(rs.field_cd, 0);
  EXPECT_EQ(rs.penalty, 40);  // a clean lap charges no route penalty

  // Back on the start cell past the line: the gate arms and the run finishes.
  EXPECT_TRUE(rs.update_finish(center(3) * 64, 28500 * 64, sf));
  EXPECT_EQ(rs.field_cd, 1);
  EXPECT_TRUE(rs.finished);
  EXPECT_EQ(rs.total_finish, 43);  // game_frame 3 + penalty 40

  // The clock freezes and no further crossing is reported.
  rs.tick();
  EXPECT_EQ(rs.game_frame, 3);
  EXPECT_TRUE(!rs.update_finish(center(3) * 64, (center(27) + 200) * 64, sf));

  rs.reset();
  EXPECT_TRUE(!rs.finished);
  EXPECT_EQ(rs.field_cd, 0);
  EXPECT_EQ(rs.game_frame, 0);
}

TEST(race_state_offtrack_penalty_keeps_node) {
  // A cell outside the 30x30 grid returns the -2 penalty, latches field_45B and
  // must not disturb field_2F4 (state.c loc_172CB / loc_173AD); returning to the
  // route clears the off-track state.
  const Track t = make_loop_track();
  const StartFinish sf = find_start_finish(t);
  RaceState rs;
  EXPECT_TRUE(rs.build_route(t, sf));

  // Drive to route node 1 so field_2F2/2F4 have left the start cell.
  rs.update_route(center(3) * 64, center(28) * 64);
  EXPECT_EQ(rs.field_2f2, 1);
  EXPECT_EQ(rs.field_2f4, 1);

  // row = 29 - (car_z >> 16) is outside the grid -> the -2 penalty.
  rs.update_route(center(3) * 64, -40 * 64);
  EXPECT_EQ(rs.field_45b, 1);  // off track
  EXPECT_EQ(rs.field_2f4, 1);  // node untouched

  // Back on the route at the next cell: the off-track latch clears. (The tick
  // must land on a cell other than the snapped start cell, or detect_penalty
  // short circuits and the machine is skipped.)
  rs.update_route(center(2) * 64, center(28) * 64);
  EXPECT_EQ(rs.field_45b, 0);
  EXPECT_EQ(rs.field_2f2, 2);
}

TEST(race_state_wrong_way_latch) {
  // Jumping back to the predecessor cell makes detect_penalty report a node at
  // distance 7; since node 3 is the predecessor of field_2F4 (= 4) the machine
  // marks the car as driving against the route (seg001.asm loc_1736A).
  const Track t = make_loop_track();
  const StartFinish sf = find_start_finish(t);
  RaceState rs;
  EXPECT_TRUE(rs.build_route(t, sf));

  // Drive forward to node 4 (file rows 28,28,28,27).
  const int fwd[4][2] = {{3, 28}, {2, 28}, {1, 28}, {1, 27}};
  for (int i = 0; i < 4; ++i) {
    rs.update_route(center(fwd[i][0]) * 64, center(fwd[i][1]) * 64);
  }
  EXPECT_EQ(rs.field_2f2, 4);
  EXPECT_EQ(rs.field_45b, 0);

  // Teleport back one node: node 3 is at (col 1, bottom-up row 1).
  rs.update_route(center(1) * 64, center(28) * 64);
  EXPECT_EQ(rs.field_45b, 2);  // wrong way
}

TEST(race_state_without_route_never_finishes) {
  // With no route graph the penalty scan cannot run, so the finish gate never
  // opens even when the car crosses the line.
  const StartFinish sf = make_sf(7, 5, 0);
  RaceState rs;
  EXPECT_TRUE(!rs.update_finish(center(7) * 64, (center(5) + 200) * 64, sf));
  EXPECT_EQ(rs.field_cd, 0);
  EXPECT_TRUE(!rs.finished);
}
