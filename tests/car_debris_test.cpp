#include <cstdint>

#include "core/kevin_random.h"
#include "physics/car_debris.h"
#include "physics/car_state.h"
#include "tiny_test.h"

using namespace s3d;
using namespace s3d::physics;

// ---------------------------------------------------------------------------
// kevinrandom (restunts restunts.c get_kevinrandom), seeded "kevin" + NUL.
// ---------------------------------------------------------------------------

TEST(kevin_random_matches_the_original_sequence) {
  KevinRandom rng("kevin");
  // Hand-derived from the generator with the seed bytes {'k','e','v','i','n',0}
  // (restunts restunts.c get_kevinrandom): the first three draws are 29, 98, 241.
  const int expected[12] = {29, 98, 241, 245, 17, 101, 148, 203, 201, 232, 39, 53};
  for (int i = 0; i < 12; ++i) {
    EXPECT_EQ(static_cast<int>(rng.next()), expected[i]);
  }

  // Re-seeding restarts the sequence.
  rng.reset("kevin");
  EXPECT_EQ(static_cast<int>(rng.next()), expected[0]);
}

// ---------------------------------------------------------------------------
// state_op_unk: the burst spawn
// ---------------------------------------------------------------------------

TEST(debris_spawn_player_burst) {
  DebrisField field;
  KevinRandom rng("kevin");
  const int spawned = spawn_debris(&field, kDebrisOwnerPlayer, 0x40, 0, &rng);

  // A car burst is capped at var_E = 18 pieces and fills every free slot.
  EXPECT_EQ(spawned, 18);
  EXPECT_EQ(field.live_count(), 18);
  EXPECT_TRUE(field.any_active());

  for (int i = 0; i < 18; ++i) {
    const DebrisPiece& p = field.pieces[i];
    EXPECT_EQ(static_cast<int>(p.owner), static_cast<int>(kDebrisOwnerPlayer));
    // field_42B = (n & 3) + owner*4 + 4: the player's own exp0..exp3.
    EXPECT_EQ(static_cast<int>(p.shape), 4 + (i & 3));
    // field_35E: (n * 0x400) / 18 + car_rotate.x, masked to the circle.
    EXPECT_EQ(static_cast<int>(p.angle), ((i * 0x400) / 18 + 0x40) & 0x3FF);
    // game_longs1/2/3 start at zero (relative to the car).
    EXPECT_EQ(p.x, 0);
    EXPECT_EQ(p.y, 0);
    EXPECT_EQ(p.z, 0);
  }

  // Golden values from the kevinrandom stream (three calls per piece:
  // field_2FE, field_32E, field_38E).
  EXPECT_EQ(static_cast<int>(field.pieces[0].spin_z), 29 * 4);
  EXPECT_EQ(static_cast<int>(field.pieces[0].spin_x), 98 * 4);
  EXPECT_EQ(static_cast<int>(field.pieces[0].speed), ((241 * 6) >> 2) + 0x180);
  EXPECT_EQ(static_cast<int>(field.pieces[0].vy), (6 * (((241 * 6) >> 2) + 0x180)) >> 2);
  EXPECT_EQ(static_cast<int>(field.pieces[1].spin_z), 245 * 4);
  EXPECT_EQ(static_cast<int>(field.pieces[1].spin_x), 17 * 4);
  EXPECT_EQ(static_cast<int>(field.pieces[1].speed), ((101 * 6) >> 2) + 0x180);
  EXPECT_EQ(static_cast<int>(field.pieces[2].spin_z), 148 * 4);
  EXPECT_EQ(static_cast<int>(field.pieces[2].spin_x), 203 * 4);
  EXPECT_EQ(static_cast<int>(field.pieces[2].speed), ((201 * 6) >> 2) + 0x180);
}

TEST(debris_spawn_scenery_burst) {
  DebrisField field;
  KevinRandom rng("kevin");
  // Owner >= 2 is a track object: 8 generic shards over a 0xC0 arc, launched
  // from `car_rotate.x - 0x60` with the impact speed added.
  const int spawned = spawn_debris(&field, 2, 0x40, 100, &rng);

  EXPECT_EQ(spawned, kDebrisSceneryPieces);
  EXPECT_EQ(field.live_count(), 8);
  for (int i = 0; i < 8; ++i) {
    const DebrisPiece& p = field.pieces[i];
    EXPECT_EQ(static_cast<int>(p.owner), 2);
    // var_10 = 0 for scenery: the generic GAME1/GAME2 shards 0..3.
    EXPECT_EQ(static_cast<int>(p.shape), i & 3);
    EXPECT_EQ(static_cast<int>(p.angle), ((i * 0xC0) / 8 + (0x40 - 0x60)) & 0x3FF);
  }
  // The rng order is identical to the car burst; only the speed gains `extra`
  // and var_8 = 1 halves the vertical component (field_3BE = speed >> 2).
  EXPECT_EQ(static_cast<int>(field.pieces[0].spin_z), 29 * 4);
  EXPECT_EQ(static_cast<int>(field.pieces[0].spin_x), 98 * 4);
  EXPECT_EQ(static_cast<int>(field.pieces[0].speed), ((241 * 6) >> 2) + 100 + 0x180);
  EXPECT_EQ(static_cast<int>(field.pieces[0].vy), (((241 * 6) >> 2) + 100 + 0x180) >> 2);
}

TEST(debris_spawn_only_fills_free_slots) {
  DebrisField field;
  KevinRandom rng("kevin");
  EXPECT_EQ(spawn_debris(&field, kDebrisOwnerPlayer, 0, 0, &rng), 18);

  // 6 slots are free, so the next 18-piece burst is truncated (var_12 = min).
  EXPECT_EQ(spawn_debris(&field, kDebrisOwnerPlayer, 0, 0, &rng), 6);
  EXPECT_EQ(field.live_count(), 24);

  // With every slot live nothing spawns.
  EXPECT_EQ(spawn_debris(&field, kDebrisOwnerPlayer, 0, 0, &rng), 0);
  EXPECT_EQ(field.live_count(), 24);
}

TEST(debris_clear_and_shape_mapping) {
  DebrisField field;
  KevinRandom rng("kevin");
  spawn_debris(&field, kDebrisOwnerPlayer, 0, 0, &rng);
  field.clear();
  EXPECT_TRUE(!field.any_active());
  EXPECT_EQ(field.live_count(), 0);

  // sceneshapes3 -> game3dshapes: 0..3 scenery shards, 4..7 the player car's,
  // 8..11 the opponent's.
  EXPECT_EQ(debris_shard_index(0), 0);
  EXPECT_EQ(debris_shard_index(3), 3);
  EXPECT_EQ(debris_shard_index(4), 0);
  EXPECT_EQ(debris_shard_index(7), 3);
  EXPECT_EQ(debris_shard_index(8), 0);
  EXPECT_EQ(debris_shard_index(11), 3);
  EXPECT_EQ(debris_archive_index(0), 0);
  EXPECT_EQ(debris_archive_index(3), 0);
  EXPECT_EQ(debris_archive_index(4), 1);
  EXPECT_EQ(debris_archive_index(7), 1);
  EXPECT_EQ(debris_archive_index(8), 2);
  EXPECT_EQ(debris_archive_index(11), 2);
}

// ---------------------------------------------------------------------------
// sub_19BA0: the per-frame motion
// ---------------------------------------------------------------------------

TEST(debris_update_moves_along_the_launch_heading) {
  DebrisField field;
  field.pieces[0].speed = 100;
  field.pieces[0].angle = 0x100;  // +90 degrees: sin = 1.0, cos = 0

  CarState car;
  car.car_position.y = 1000000;  // well above sea level
  update_debris(&field, car, 20);

  // out = mat_rot_zxy(0, 0, 0x100, 1) * (0, 0, 100) = (100, 0, 0): the piece
  // travels along the world +x axis.
  EXPECT_EQ(field.pieces[0].x, 100);
  EXPECT_EQ(field.pieces[0].z, 0);
  // Gravity is the only vertical motion.
  EXPECT_EQ(static_cast<int>(field.pieces[0].vy), -kDebrisGravity);
  EXPECT_EQ(field.pieces[0].y, -kDebrisGravity);
  // Surviving pieces keep tumbling.
  EXPECT_EQ(static_cast<int>(field.pieces[0].spin_z), kDebrisSpinStep);
  EXPECT_EQ(static_cast<int>(field.pieces[0].spin_x), kDebrisSpinStep);
  EXPECT_TRUE(field.any_active());
}

TEST(debris_update_angle_zero_moves_along_z) {
  DebrisField field;
  field.pieces[5].speed = 100;
  field.pieces[5].angle = 0;  // sin = 0, cos = 1.0

  CarState car;
  car.car_position.y = 1000000;
  update_debris(&field, car, 20);

  EXPECT_EQ(field.pieces[5].x, 0);
  EXPECT_EQ(field.pieces[5].z, 100);
}

TEST(debris_update_retires_pieces_below_sea_level) {
  DebrisField field;
  field.pieces[3].speed = 100;
  field.pieces[3].angle = 0x100;
  field.pieces[3].y = -50;
  field.pieces[3].spin_z = 7;

  CarState car;
  car.car_position.y = 0;  // sea level
  update_debris(&field, car, 20);

  // y = -50 - 19 = -69, and -69 + 0 < 0: the piece is retired (field_38E = 0)
  // and stops tumbling.
  EXPECT_EQ(static_cast<int>(field.pieces[3].speed), 0);
  EXPECT_TRUE(!field.pieces[3].active());
  EXPECT_TRUE(!field.any_active());
  EXPECT_EQ(static_cast<int>(field.pieces[3].spin_z), 7);
}

TEST(debris_update_keeps_pieces_above_sea_level) {
  DebrisField field;
  field.pieces[0].speed = 10;
  field.pieces[0].angle = 0;
  field.pieces[0].y = -50;

  CarState car;
  car.car_position.y = 100;  // -50 + 100 >= 0
  update_debris(&field, car, 20);

  EXPECT_EQ(static_cast<int>(field.pieces[0].speed), 10);
  EXPECT_TRUE(field.pieces[0].active());
  EXPECT_EQ(static_cast<int>(field.pieces[0].vy), -kDebrisGravity);
}

TEST(debris_update_applies_gravity_twice_at_10hz) {
  DebrisField field;
  field.pieces[0].speed = 10;
  field.pieces[0].angle = 0;

  CarState car;
  car.car_position.y = 1000000;
  update_debris(&field, car, 10);

  EXPECT_EQ(static_cast<int>(field.pieces[0].vy), -2 * kDebrisGravity);
  EXPECT_EQ(field.pieces[0].y, -3 * kDebrisGravity);
}

TEST(debris_update_ignores_free_slots) {
  DebrisField field;  // every slot has speed 0

  CarState car;
  car.car_position.y = 0;
  update_debris(&field, car, 20);

  EXPECT_TRUE(!field.any_active());
  EXPECT_EQ(field.pieces[0].x, 0);
  EXPECT_EQ(field.pieces[23].y, 0);
}

// A whole player burst eventually lands and retires: 18 pieces, then no live
// slots, and the "any live" latch (field_42A) drops.
TEST(debris_burst_lands_and_retires) {
  DebrisField field;
  KevinRandom rng("kevin");
  EXPECT_EQ(spawn_debris(&field, kDebrisOwnerPlayer, 0, 0, &rng), 18);

  CarState car;
  car.car_position.y = 0;
  for (int frame = 0; frame < 200 && field.any_active(); ++frame) {
    update_debris(&field, car, 20);
  }
  EXPECT_TRUE(!field.any_active());
  EXPECT_EQ(field.live_count(), 0);
}
