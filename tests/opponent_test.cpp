// Opponent definition loading (OPPn.PRE) and the field_3F9 speed lookup inputs.

#include <string>

#include "assets/opponent.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/track_route.h"

using namespace s3d;
using namespace s3d::world;

TEST(opponent_file_names) {
  EXPECT_EQ(opponent_file(1), std::string("OPP1.PRE"));
  EXPECT_EQ(opponent_file(6), std::string("OPP6.PRE"));
  EXPECT_EQ(opponent_file(0), std::string("OPP1.PRE"));  // clamped
  EXPECT_EQ(opponent_file(9), std::string("OPP6.PRE"));  // clamped
}

TEST(opponent_loads_speed_table) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }

  OpponentData opp;
  EXPECT_TRUE(opp.load(path_join(dir, "OPP1.PRE")));
  // OPP1 sped = {120,75,50,40,30,20,60,40,20,90,115,20,30,70,80,50}.
  const auto& sped = opp.speed_table();
  EXPECT_EQ(sped[0], 120);
  EXPECT_EQ(sped[10], 115);
  EXPECT_EQ(sped[15], 50);
}

TEST(opponent_speed_for_indexes_table_and_guards_range) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  OpponentData opp;
  EXPECT_TRUE(opp.load(path_join(dir, "OPP1.PRE")));

  EXPECT_EQ(opp.speed_for(0, 0), 120);  // sped[0]
  EXPECT_EQ(opp.speed_for(9, 1), 115);  // sped[10]
  EXPECT_EQ(opp.speed_for(14, 0), 80);  // sped[14]
  EXPECT_EQ(opp.speed_for(14, 1), 50);  // sped[15]
  // speed_code 15 + surface 1 = 16 would run past the 16-byte table.
  EXPECT_EQ(opp.speed_for(15, 1), 0);
  EXPECT_EQ(opp.speed_for(15, 2), 0);
}

TEST(opponent_engine_torque_from_speed0) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  OpponentData opp;
  EXPECT_TRUE(opp.load(path_join(dir, "OPP1.PRE")));
  // sped[0] = 120 -> (0xC8 - 120) >> 1 = 40.
  EXPECT_EQ(opp.engine_torque(), 40);
}

TEST(opponent_missing_file_fails) {
  OpponentData opp;
  EXPECT_TRUE(!opp.load("/nonexistent/OPP1.PRE"));
}

TEST(route_checkpoint_exposes_speed_code_and_surface) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  Track track = Track::load(path_join(dir, "DEFAULT.TRK"));
  if (track.items[0] == 0 && track.items[100] == 0) {
    std::printf("  (skipped: no track)\n");
    return;
  }
  // Find the start/finish cell from the start/finish shape (element 1).
  int sc = -1, sr = -1;
  for (int r = 0; r < Track::kSize && sc < 0; ++r) {
    for (int c = 0; c < Track::kSize; ++c) {
      if (track.item(c, r) == 1) {
        sc = c;
        sr = r;
        break;
      }
    }
  }
  if (sc < 0) {
    std::printf("  (skipped: no start/finish)\n");
    return;
  }
  TrackRoute route;
  EXPECT_TRUE(route.build(track, sc, sr, 0x000));
  if (!route.valid()) return;
  RouteCheckpoint cp;
  const bool last = route.checkpoint(0, 0, track, &cp);
  (void)last;
  // The straight start element (1) has no sped code; just make sure the fields
  // are populated (they are raw table inputs, so any in-range value is fine).
  EXPECT_TRUE(cp.speed_code <= 15);
  EXPECT_TRUE(cp.surface == 0 || cp.surface == 1 || cp.surface == 2 ||
              cp.surface == 0xFF);
}
