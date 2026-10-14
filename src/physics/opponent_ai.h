#pragma once
// Opponent car AI (seg001.asm `opponent_op`, lines 77-811): the per-frame
// brain that drives the opponent car selected in the garage, plus the physics
// tail it applies (update_car_speed / update_grip / update_player_state).
//
// The port follows the original assembly literally, keeping its quirks:
//   * the 0x400-unit heading is the polar angle of the *rotated* checkpoint
//     vector (fake 3D for the 0xFFFF sentinel y = reverse/swooping blocks),
//   * navigation targets the player-car side only while the player is beside
//     the route; otherwise it aims at the checkpoint midpoint,
//   * a turn "too sharp" (|heading| > 0x100) or an out-of-envelope steering
//     request advances the course index (loc_14866), as does arriving within
//     0xC8 units of a checkpoint,
//   * the steering angle steps by 8/16 per frame (20/10 fps) toward a target
//     clamped to +-0x41, and the aim clamp on air is cleared by the front
//     wheel contact sum,
//   * the pedal decision is {coast, brake, accelerate} and the crashed car
//     brakes; a spinning wheel (car_velocity_heading_offset) sheds speed by
//     var_10*0x200 per frame instead,
//   * the off-track crash band (cos/sin of the start cell) only triggers from
//     lap one onwards (field_CD != 0).
//
// The car-vs-car collision is intentionally NOT part of this module: the
// original runs it inside each car's update_player_state, and the integration
// (drive scene) applies it with car_car_resolve() for the player's and the
// opponent's pass, preserving the per-contact latch.

#include <cstdint>
#include <vector>

#include "assets/car.h"
#include "assets/opponent.h"
#include "physics/car_motion.h"
#include "physics/car_state.h"
#include "world/track_route.h"

namespace s3d {
struct Track;
}

namespace s3d::physics {

class TrackCollision;

// Per-race opponent AI state (the original's game-global `state`).
struct OpponentAiState {
  // state.field_3F9: the opponent's current per-element speed-code target.
  // sub_18D60 stores oppnentSped[speed_code + surface] here every time the AI
  // advances to the next course checkpoint; opponent_op reads it each frame.
  uint8_t target_speed_code = 0;
};

// Everything the AI needs that is not carried persistently in CarState.
struct OpponentRaceSetup {
  const world::TrackRoute* route = nullptr;  // checkpoint() table
  const Track* track = nullptr;              // terrain lookup for checkpoint()
  const OpponentData* data = nullptr;        // sped table + engine torque
  std::vector<uint16_t> path;                // trackdata3 mirror (0, 0, 1 tail)
  int start_col = 0;                         // startcol2
  int start_row = 0;                         // startrow2
  int track_angle = 0;                       // start/finish direction (0..0x300)
  int game_inputmode = 0;                    // 0 live race, 2 = menu autopilot
};

// Race-setup equivalent of init_game_state's opponent block (restunts.c:572),
// i.e. the first sub_18D60 call: writes the path node 0 / index 0 checkpoint
// into `opp`'s route fields, records field_3F9, and leaves
// car_route_point_index at 1 (the original's post-call field_CE++). Call once
// before the first run_opponent_ai().
void init_opponent_checkpoint(const OpponentRaceSetup& setup, OpponentAiState* ai,
                              CarState* opp);

// Advances the opponent car one physics tick (seg001.asm opponent_op and its
// physics tail). `opp` is the car being driven; `player` is read for relative
// positions and crash state (interception checks). `opp_spec` must be the
// opponent's SIMD profile and `frames_per_sec` selects the 20/10 fps steering
// and spin step. Returns state.field_45E: 0 = no interception, 1 = got ahead
// on the first-edge side, 2 = got ahead on the second-edge side.
uint8_t run_opponent_ai(const OpponentRaceSetup& setup, OpponentAiState* ai,
                        CarState* opp, const CarState& player, const CarSpec& opp_spec,
                        int frames_per_sec, const GroundSampler& ground,
                        const TrackCollision* collision);

}  // namespace s3d::physics