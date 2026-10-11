#pragma once
// Race loop: start/finish line resolution, the 20 Hz race clock, the finish
// crossing test and the original time formatting.
//
// Ported from:
//   * seg004.asm loc_2082C..loc_20966   start/finish element scan
//   * seg001.asm detect_penalty (5307)  route node location (start-cell rule)
//   * state.c     loc_172CB..loc_17771 field_CD route gate + finish test
//   * statecrs.c  loc_19730             finish: crashBmpFlag 3, total_finish
//   * seg008.asm  format_frame_as_string / print_int_as_string_maybe

#include <cstdint>
#include <string>

#include "assets/trk.h"

namespace s3d::game {

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

// Clock and finish state for one run. The original keeps these in GAMESTATE
// (game_frame / game_penalty / game_total_finish) and CARSTATE.car_crashBmpFlag.
struct RaceState {
  int game_frame = 0;    // 20 Hz frames since the run started
  int penalty = 0;       // game_penalty, in frames (route penalties, deferred)
  bool finished = false;
  int total_finish = 0;  // game_total_finish, in frames

  // playerstate.field_CD counts the times the car has returned to route node 0
  // (the start/finish cell) after passing any other node; the finish test only
  // runs once it is non-zero (state.c loc_17771). field_2F2 / field_2F4 are the
  // current and previous route node the penalty scan located the car on. The
  // full route graph (seg004.asm track_setup / detect_penalty) is not ported
  // yet, so only the distinction that drives field_CD is tracked here: route
  // node 0 (the start cell) versus "any other node" (1).
  int field_cd = 0;
  int field_2f2 = 0;
  int field_2f4 = 0;

  void reset() { *this = RaceState{}; }

  // Call once per 20 Hz physics tick, after the car has moved.
  void tick() {
    if (!finished) ++game_frame;
  }

  // Detect the car's route node for this tick and advance field_CD, mirroring
  // state.c loc_172CB..loc_1737B over the reduced detect_penalty result:
  //   * the start cell maps to route node 0; any other in-bounds cell is some
  //     other node (tracked as 1);
  //   * a cell outside the 30x30 grid yields the -2 "off track" penalty, which
  //     leaves the node (and field_CD) untouched;
  //   * field_CD increments when the located node is 0 and the previous located
  //     node was not (the car has left the start cell and come back).
  void update_route(int32_t car_x_long, int32_t car_z_long,
                    const StartFinish& sf) {
    if (!sf.found) return;
    // detect_penalty col = world_x >> 10, row = 29 - (world_z >> 10); the car
    // position is 1/64 world units, so the cells are the >>16 bytes.
    const int col = static_cast<int>(car_x_long >> 16);
    const int row = 29 - static_cast<int>(car_z_long >> 16);
    if (col < 0 || col > 29 || row < 0 || row > 29) {
      field_2f4 = field_2f2;  // var_2 survives the -2 penalty untouched
      return;
    }
    const bool at_start = col == sf.col && row == (29 - sf.row);
    if (at_start) {
      if (field_2f4 != 0) ++field_cd;
      field_2f2 = 0;
      field_2f4 = 0;
    } else {
      field_2f2 = 1;
      field_2f4 = 1;
    }
  }

  // Tests the finish line for the current car position (state.c loc_17771).
  // Returns true on the tick the car crosses it forward once the route gate
  // (field_CD) is open, transitioning to the finished state.
  bool update_finish(int32_t car_x_long, int32_t car_z_long,
                     const StartFinish& sf) {
    if (finished || !sf.found) return false;
    update_route(car_x_long, car_z_long, sf);
    if (field_cd == 0) return false;
    const int dist = finish_line_distance(car_x_long, car_z_long, sf);
    if (dist < 0) {
      finished = true;
      total_finish = game_frame + penalty;  // + elapsed_time1 (0 in one run)
      return true;
    }
    return false;
  }
};

}  // namespace s3d::game
