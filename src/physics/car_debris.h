#pragma once
// Crash debris: the small flat shards a car (or a smashed track object) throws
// off in the original engine.
//
// Reference: reference/restunts asmorig/seg001.asm:9215 (`state_op_unk`, the
// spawn) and :9386 (`sub_19BA0`, the per-frame motion), drawn in
// asmorig/seg003.asm (frame.c ~903).
//
// The 24 pieces live in fixed slots shared by the player, the opponent and the
// smashed scenery, exactly as in the original `state.*` arrays:
//
//   game_longs1/2/3[24]  position, in 1/64 world units, relative to the owning
//                        car (the renderer adds the car's world position and
//                        shifts right by 6). x/z are horizontal, y is height.
//   field_2FE[24]        tumble about the shape's local z (1/1024 turn)
//   field_32E[24]        tumble about the shape's local x
//   field_35E[24]        launch heading, evenly spread over the burst arc
//   field_38E[24]        horizontal launch speed (1/64 units per frame);
//                        **0 marks the slot free**
//   field_3BE[24]        vertical speed; gravity is subtracted each frame
//   field_42B[24]        shape index into the original's `sceneshapes3` table
//   field_443[24]        owner: 0 player, 1 opponent, >= 2 a track object
//   field_42A            "any slot live", re-derived every frame
//
// `sceneshapes3` (dseg.asm) resolves its `shape` offsets through the
// `game3dshapes` table, which gives:
//
//   field_42B 0..3    "exp0".."exp3" of GAME1/GAME2 (generic scenery shards)
//   field_42B 4..7    the player car's own "exp0".."exp3" (ST<ID>.P3S)
//   field_42B 8..11   the opponent car's "exp0".."exp3"
//
// so the caller picks the ShapeLibrary from the shape index (see
// `debris_archive_index` / `debris_shard_index`). This module owns the
// simulation only; the scene builds the meshes and draws each live piece.

#include <cstdint>

#include "core/kevin_random.h"
#include "physics/car_state.h"

namespace s3d::physics {

// Size of the original `state.game_longs1[24]` arrays.
inline constexpr int kDebrisSlots = 24;

// state_op_unk's per-owner piece cap (`var_E`): 18 for a car, 8 for scenery.
inline constexpr int kDebrisCarPieces = 0x12;
inline constexpr int kDebrisSceneryPieces = 8;

// Gravity subtracted from field_3BE each frame (`sub word ptr [bx], 13h`).
inline constexpr int kDebrisGravity = 0x13;
// Per-frame tumble added to field_2FE/field_32E (`add ..., 10h`).
inline constexpr int kDebrisSpinStep = 0x10;
// field_38E is `((rnd*6)>>2) + extra + 180h`, so a fresh piece always starts
// moving at least 0x180.
inline constexpr int kDebrisBaseSpeed = 0x180;

// Owner id stored in field_443.
inline constexpr uint8_t kDebrisOwnerPlayer = 0;
inline constexpr uint8_t kDebrisOwnerOpponent = 1;
// Owners >= this are track objects (the original passes `si + 2`).
inline constexpr uint8_t kDebrisOwnerTrackObjectBase = 2;

struct DebrisPiece {
  int32_t x = 0;        // game_longs1: 1/64 world units, relative to the owner car
  int32_t y = 0;        // game_longs2 (height)
  int32_t z = 0;        // game_longs3
  int16_t spin_z = 0;   // field_2FE
  int16_t spin_x = 0;   // field_32E
  int16_t angle = 0;    // field_35E
  int16_t speed = 0;    // field_38E; 0 when the slot is free
  int16_t vy = 0;       // field_3BE
  uint8_t shape = 0;    // field_42B
  uint8_t owner = 0;    // field_443

  bool active() const { return speed != 0; }
};

struct DebrisField {
  DebrisPiece pieces[kDebrisSlots];

  void clear();
  bool any_active() const;
  int live_count() const;
};

// Which of the caller's four "exp0".."exp3" shapes a piece uses: the shape
// index's low two bits (state_op_unk sets `field_42B = (n & 3) + var_10`, and
// var_10 is 0 or 4, so the low bits identify the shard within the burst).
inline int debris_shard_index(uint8_t shape) { return shape & 3; }
// Which archive the piece belongs to: 0 = GAME1/GAME2 scenery shards,
// 1 = the player car's ST<ID>.P3S, 2 = the opponent's.
inline int debris_archive_index(uint8_t shape) { return shape >> 2; }

// `state_op_unk(owner, car_rotate_x, extra)`: fills free slots with one burst
// of shards and returns how many were spawned.
//
//   owner        field_443: 0 player, 1 opponent, >= 2 a track object
//   car_yaw      the owner car's `car_rotate.x` (1/1024 turn): the base heading
//                of the burst (the scenery path uses `car_yaw - 0x60`)
//   extra        added to every piece's launch speed; the track-object path
//                passes the impact-speed term here, car crashes pass 0
//   rng          consumed three times per piece, in the original order
//
// A car burst is up to 18 pieces spread over the full circle; a scenery burst
// is up to 8 pieces spread over 0xC0 (a 67-degree arc).
int spawn_debris(DebrisField* field, int owner, int16_t car_yaw, int16_t extra,
                 KevinRandom* rng);

// `sub_19BA0`: advances every live piece one 20 Hz frame and retires the ones
// that fall below sea level, then refreshes the "any live" latch.
//
// The original tests the *player* car's height for every piece, even the
// opponent's and the scenery's, so `player` is always the player's state.
// `frames_per_sec` is the game's frame rate (game_frames_per_sec): at the
// original's 10 Hz slow-motion gravity is applied twice per tick.
void update_debris(DebrisField* field, const CarState& player, int frames_per_sec);

}  // namespace s3d::physics
