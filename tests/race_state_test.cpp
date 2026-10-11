// Race loop tests: the start/finish element scan (seg004.asm), the finish-line
// signed-distance test (state.c) and the format_frame_as_string clock format.

#include <string>

#include "assets/trk.h"
#include "game/race_state.h"
#include "tiny_test.h"

using namespace s3d;
using namespace s3d::game;

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

TEST(race_state_finish_transition) {
  const StartFinish sf = make_sf(7, 5, 0);
  const int cz = center(5);
  RaceState rs;
  rs.tick();
  rs.tick();
  rs.tick();
  EXPECT_EQ(rs.game_frame, 3);
  rs.penalty = 40;

  // First sighting of the car in front of the line arms the test.
  EXPECT_TRUE(!rs.update_finish(0, (cz - 210) * 64, sf));
  EXPECT_TRUE(rs.armed);
  EXPECT_TRUE(!rs.finished);

  // Crossing forward finishes the run and records the total.
  EXPECT_TRUE(rs.update_finish(0, (cz + 10) * 64, sf));
  EXPECT_TRUE(rs.finished);
  EXPECT_EQ(rs.total_finish, 43);

  // The clock freezes and no further crossing is reported.
  rs.tick();
  EXPECT_EQ(rs.game_frame, 3);
  EXPECT_TRUE(!rs.update_finish(0, (cz + 200) * 64, sf));

  rs.reset();
  EXPECT_TRUE(!rs.finished);
  EXPECT_TRUE(!rs.armed);
  EXPECT_EQ(rs.game_frame, 0);
}
