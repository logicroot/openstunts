#pragma once
// Race loop: start/finish line resolution, the 20 Hz race clock, the finish
// crossing test and the original time formatting.
//
// Ported from:
//   * seg004.asm loc_2082C..loc_20966   start/finish element scan
//   * state.c     loc_17771..loc_17810  finish-line signed-distance test
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
  bool armed = false;    // ~ field_CD != 0: the car has left the start line

  void reset() { *this = RaceState{}; }

  // Call once per 20 Hz physics tick, after the car has moved.
  void tick() {
    if (!finished) ++game_frame;
  }

  // Tests the finish line for the current car position. Returns true on the
  // tick the car crosses it forward, transitioning to the finished state.
  bool update_finish(int32_t car_x_long, int32_t car_z_long,
                     const StartFinish& sf) {
    if (finished || !sf.found) return false;
    const int dist = finish_line_distance(car_x_long, car_z_long, sf);
    if (!armed) {
      // The car spawns just behind the line (distance > 0). The original arms
      // the test once the first route node is passed (playerstate.field_CD);
      // here, leaving the line serves as the equivalent guard.
      if (dist > 0) armed = true;
      return false;
    }
    if (dist < 0) {
      finished = true;
      total_finish = game_frame + penalty;  // + elapsed_time1 (0 in one run)
      return true;
    }
    return false;
  }
};

}  // namespace s3d::game
