#pragma once
// Race loop: start/finish line resolution, the 20 Hz race clock, the finish
// crossing test and the original time formatting.
//
// Ported from:
//   * seg004.asm loc_2082C..loc_20966   start/finish element scan
//   * seg004.asm track_setup            route graph (world/track_route.h)
//   * seg001.asm detect_penalty (5307)  route node location + penalty distance
//   * state.c     loc_172CB..loc_173AD  penalty machine (field_45B/45C/2F2/2F4)
//   * state.c     loc_17771             field_CD finish gate
//   * statecrs.c  loc_19730             finish: crashBmpFlag 3, total_finish
//   * seg008.asm  format_frame_as_string / print_int_as_string_maybe

#include <cstdint>
#include <string>

#include "assets/trk.h"
#include "world/track_route.h"

namespace s3d::game {

// Runtime game frame rate (the original's `framespersec`).
inline constexpr int kRaceFrameRate = 20;

// The track's start/finish element, resolved by scanning the item grid.
struct StartFinish {
  bool found = false;
  bool multiple = false;  // more than one start/finish element (invalid track)
  int col = -1;
  int row = -1;
  int angle = 0;  // track direction, 0x400 units (seg004.asm)
};

// The 0x400-unit direction of a start/finish element byte, or -1 when the item
// is not a start/finish line.
int start_finish_angle(uint8_t element);

// Scans the item grid for the (single) start/finish line. `multiple` is set
// when a second one is found, mirroring the original's `many_sf` track error.
StartFinish find_start_finish(const Track& track);

// Signed distance from the car (1/64 world units) to the start/finish line
// along the track direction; negative once the car has crossed it forward.
// Returns +1 (never crossed) when no line was found.
int finish_line_distance(int32_t car_x_long, int32_t car_z_long,
                         const StartFinish& sf);

// restunts format_frame_as_string: minutes space-padded to two columns, seconds
// zero-padded; with `hundredths`, appends "." plus (100 / frames_per_sec) times
// the remaining frames.
std::string format_race_time(int frames, int frames_per_sec, bool hundredths);

// One `detect_penalty` lookup (seg001.asm). The original returns a node and a
// distance through its two out-pointers; `located` is the boolean return.
struct PenaltyProbe {
  bool located = false;  // false: the car sits on the start cell (asm ret 0)
  int node = 0;          // var_2, the located route node (or the input node)
  int penalty = 0;       // var_1E distance; -1 wrapped onto node 0, -2 off track
};

// Clock, route and finish state for one run. The original keeps these split
// across GAMESTATE (game_frame / game_penalty / game_total_finish), GAMESTATE's
// playerstate (field_CD / field_2F2 / field_2F4 / field_45B / field_45C) and the
// game_startcol/2 - game_startrow/2 start-cell pairs (detect_penalty state),
// plus CARSTATE.car_crashBmpFlag on finish.
struct RaceState {
  // The route graph built by `build_route`; empty until that succeeds.
  world::TrackRoute route;

  int game_frame = 0;    // 20 Hz frames since the run started
  int penalty = 0;       // game_penalty, in frames (route penalties)
  bool finished = false;
  int total_finish = 0;  // game_total_finish, in frames

  int field_cd = 0;   // playerstate.field_CD: finish gate (see update_finish)
  int field_2f2 = 0;  // current located node
  int field_2f4 = 0;  // previous located node
  int field_45b = 0;  // 0 normal, 1 off track (penalty -2), 2 wrong way
  int field_45c = 0;  // consecutive "wrong direction" ticks
  int show_penalty_counter = 0;  // HUD "WRONG WAY"/"OFF ROAD" banner frames

  // game_startcol/2 and game_startrow/2: the cell the penalty scan last snapped
  // to. The car sitting on it short-circuits the scan (detect_penalty early
  // out). Initialised to the start/finish cell at route build (restunts.c 537).
  int start_col = 0;
  int start_col2 = 0;
  int start_row = 0;
  int start_row2 = 0;

  // Builds the route graph from the track and seeds the start-cell state
  // (seg004.asm track_setup + restunts.c game_startcol init). Returns false when
  // the track cannot be walked.
  bool build_route(const Track& track, const StartFinish& sf);

  // Resets the run (clock, penalty machine, start cell); the route graph is
  // kept so "R" can restart without walking the track again.
  void reset();

  // Call once per 20 Hz physics tick, after the car has moved.
  void tick();

  // Runs detect_penalty plus the penalty machine for this tick's car position
  // (1/64 world units). No-op when the route graph is not built.
  void update_route(int32_t car_x_long, int32_t car_z_long);

  // Tests the finish line for the current car position (state.c loc_17771).
  // Returns true on the tick the car crosses it forward once the route gate
  // (field_CD) is open, transitioning to the finished state.
  bool update_finish(int32_t car_x_long, int32_t car_z_long,
                     const StartFinish& sf);

 private:
  // seg001.asm detect_penalty: walks the route from `node_in` searching for the
  // node covering cell (col, row) in the original bottom-up rows.
  PenaltyProbe detect_penalty(int node_in, int col, int row);

  // state.c loc_1737B..loc_173AD: adopt `node`, clear the wrong-way run and
  // charge the route penalty for this tick.
  void apply_penalty(int node, int pen);
};

}  // namespace s3d::game
