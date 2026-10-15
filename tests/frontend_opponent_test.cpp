// Front-end opponent-selection tests.
//
// The Opponent screen has no driver list: the face view is the selection and
// Last/Next walk through it. The choice is stored on Config as an index where 0
// means Race against the Clock, which the race scene represents with an empty
// opponent car. These tests pin that encoding down; constructing the scene
// never touches GL (only on_enter/init do).

#include <string>

#include "core/config.h"
#include "game/frontend_scene.h"
#include "tiny_test.h"

using namespace s3d;
using namespace s3d_test;

namespace {

Config make_config() {
  Config config;
  config.player_car = "COUN";
  config.opponent_car.clear();
  config.opponent_type = 1;
  return config;
}

}  // namespace

// An unset opponent car is the clock, regardless of the stored driver index.
TEST(frontend_opponent_defaults_to_clock) {
  Config config = make_config();
  FrontEndScene scene(&config, "", "/nonexistent");
  EXPECT_EQ(scene.current_opponent_index(), 0);
}

// Picking a driver records both the driver index and a car to race.
TEST(frontend_opponent_select_driver) {
  Config config = make_config();
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.set_opponent(3);
  EXPECT_EQ(scene.current_opponent_index(), 3);
  EXPECT_EQ(config.opponent_type, 3);
  EXPECT_EQ(config.opponent_car, "COUN");
}

// Cycling wraps through the clock: 6 -> clock -> 1.
TEST(frontend_opponent_cycle_wraps_through_clock) {
  Config config = make_config();
  FrontEndScene scene(&config, "", "/nonexistent");

  scene.set_opponent(6);
  scene.cycle_opponent(1);
  EXPECT_EQ(scene.current_opponent_index(), 0);
  EXPECT_TRUE(config.opponent_car.empty());

  scene.cycle_opponent(1);
  EXPECT_EQ(scene.current_opponent_index(), 1);
  EXPECT_EQ(config.opponent_car, "COUN");
}

// Cycling backwards from the clock lands on the last driver.
TEST(frontend_opponent_cycle_backwards) {
  Config config = make_config();
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.cycle_opponent(-1);
  EXPECT_EQ(scene.current_opponent_index(), 6);
  EXPECT_EQ(config.opponent_type, 6);
}

// The Clock row clears the opponent car.
TEST(frontend_opponent_clock_row_clears_car) {
  Config config = make_config();
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.set_opponent(4);
  EXPECT_TRUE(!config.opponent_car.empty());
  scene.set_opponent(0);
  EXPECT_TRUE(config.opponent_car.empty());
  EXPECT_EQ(scene.current_opponent_index(), 0);
}

// Selecting a driver when the opponent already has a car keeps that car.
TEST(frontend_opponent_keeps_existing_car) {
  Config config = make_config();
  config.opponent_car = "VETT";
  FrontEndScene scene(&config, "", "/nonexistent");
  scene.set_opponent(2);
  EXPECT_EQ(config.opponent_car, "VETT");
  EXPECT_EQ(scene.current_opponent_index(), 2);
}
