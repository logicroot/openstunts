#include "game/race_state.h"

#include <array>

#include "physics/fixed_math.h"
#include "physics/wrap_math.h"
#include "world/track_objects.h"
#include "world/world_units.h"

namespace s3d::game {

namespace {

// Upper bound on the route graph size (seg004.asm's 0x385 arrays).
constexpr int kMaxRouteNodes = 0x385;

// 1.14 fixed-point sin/cos of a 0x400-unit angle (the original sin_fast).
int16_t sin_fast(int a) { return fixed::sin_fast(static_cast<unsigned short>(a)); }
int16_t cos_fast(int a) { return fixed::cos_fast(static_cast<unsigned short>(a)); }

// seg004.asm maps filler bytes before comparing against the start/finish set:
// element >= 0xFD becomes blank, 0xB6..0xFC becomes 4 (a plain road tile).
uint8_t normalize_element(uint8_t e) {
  if (e >= 0xFD) return 0;
  if (e >= 0xB6) return 4;
  return e;
}

std::string pad_number(int value, int width, char fill) {
  std::string s = std::to_string(value);
  if (static_cast<int>(s.size()) < width) {
    s.insert(0, static_cast<size_t>(width) - s.size(), fill);
  }
  return s;
}

}  // namespace

RaceSpawnDelta race_spawn_delta(int angle, int lateral) {
  // init_game_state (restunts.c): 210 units behind the line along the track
  // direction plus a 36-unit lateral flank at `lateral`.
  RaceSpawnDelta out;
  out.dx = fixed::multiply_and_scale(sin_fast(angle + 0x200), 210) +
           fixed::multiply_and_scale(sin_fast(angle + lateral), 36);
  out.dz = fixed::multiply_and_scale(cos_fast(angle + 0x200), 210) +
           fixed::multiply_and_scale(cos_fast(angle + lateral), 36);
  return out;
}

int start_finish_angle(uint8_t element) {
  // seg004.asm loc_2082C..loc_20952 (element byte -> track direction).
  switch (element) {
    case 0x01:
    case 0x86:
    case 0x93:
      return 0x000;
    case 0x87:
    case 0x94:
    case 0xB3:
      return 0x200;
    case 0x88:
    case 0x95:
    case 0xB4:
      return 0x100;
    case 0x89:
    case 0x96:
    case 0xB5:
      return 0x300;
    default:
      return -1;
  }
}

StartFinish find_start_finish(const Track& track) {
  StartFinish sf;
  for (int row = 0; row < Track::kSize; ++row) {
    for (int col = 0; col < Track::kSize; ++col) {
      const int angle = start_finish_angle(normalize_element(track.item(col, row)));
      if (angle < 0) continue;
      if (sf.found) {
        // The original aborts track setup with `many_sf`; report it so the
        // caller can fall back to a default spawn.
        sf.multiple = true;
        return sf;
      }
      sf.found = true;
      sf.col = col;
      sf.row = row;
      sf.angle = angle;
    }
  }
  return sf;
}

int finish_line_distance(int32_t car_x_long, int32_t car_z_long,
                         const StartFinish& sf) {
  if (!sf.found) return 1;
  // trackcenterpos[row] / trackcenterpos2[col] are the cell centres; car
  // positions are 1/64 world units (the original shifts them right by 6).
  const int center_x = sf.col * static_cast<int>(world::kSquareSize) +
                       static_cast<int>(world::kSquareSize / 2);
  const int center_z = sf.row * static_cast<int>(world::kSquareSize) +
                       static_cast<int>(world::kSquareSize / 2);
  const int16_t car_x = fixed::wrap16(car_x_long >> 6);
  const int16_t car_z = fixed::wrap16(car_z_long >> 6);
  const int16_t dz = fixed::wrap16(center_z - static_cast<int>(car_z));
  const int16_t dx = fixed::wrap16(center_x - static_cast<int>(car_x));
  const int16_t c = fixed::cos_fast(static_cast<unsigned short>(sf.angle));
  const int16_t s = fixed::sin_fast(static_cast<unsigned short>(sf.angle));
  return physics::wrap_add(fixed::multiply_and_scale(c, dz),
                           fixed::multiply_and_scale(s, dx));
}

std::string format_race_time(int frames, int frames_per_sec, bool hundredths) {
  if (frames < 0) frames = 0;
  const int fps = frames_per_sec > 0 ? frames_per_sec : 1;
  const int per_minute = 60 * fps;
  const int minutes = frames / per_minute;
  int rem = frames - per_minute * minutes;
  const int seconds = rem / fps;
  rem -= fps * seconds;

  // print_int_as_string_maybe: minutes left-padded with spaces to two columns,
  // seconds/padded with zeros; the hundredths field is (100 / fps) * frames.
  std::string out = pad_number(minutes, 2, ' ') + ":" + pad_number(seconds, 2, '0');
  if (hundredths) {
    out += "." + pad_number((100 / fps) * rem, 2, '0');
  }
  return out;
}

// ---------------------------------------------------------------------------
// Route setup and the wrong-way / off-track penalty scan.

bool RaceState::build_route(const Track& track, const StartFinish& sf) {
  if (!sf.found) return false;
  // restunts.c 537: game_startcol/2 and game_startrow/2 start on the line.
  start_col = start_col2 = sf.col;
  start_row = start_row2 = Track::kSize - 1 - sf.row;
  if (!route.build(track, sf.col, Track::kSize - 1 - sf.row, sf.angle)) {
    return false;
  }
  reset();
  return true;
}

void RaceState::reset() {
  game_frame = 0;
  penalty = 0;
  finished = false;
  total_finish = 0;
  field_cd = 0;
  field_2f2 = 0;
  field_2f4 = 0;
  field_45b = 0;
  field_45c = 0;
  show_penalty_counter = 0;
  penalty_time = 0;
  field_45d = 0;
  field_48 = 0;
  car_trackdata3_index = -1;
  field_ce = 0;
  car_vec_x = car_vec_y = car_vec_z = 0;
  start_col = start_col2;
  start_row = start_row2;
}

void RaceState::tick() {
  if (show_penalty_counter != 0) --show_penalty_counter;
  if (!finished) ++game_frame;
}

void RaceState::apply_penalty(int node, int pen) {
  // state.c loc_1737B..loc_173AD.
  field_2f2 = node;
  field_45c = 0;
  if (pen > 0) {
    penalty_time = pen * kRaceFrameRate * 3;
    penalty += penalty_time;
    show_penalty_counter = kRaceFrameRate << 2;
  }
  field_2f4 = node;
}

PenaltyProbe RaceState::detect_penalty(int node_in, int col, int row) {
  // seg001.asm detect_penalty (5307). `col` / `row` are the car cell in the
  // original bottom-up rows; `row` = var_A, `col` = var_4.
  PenaltyProbe out;
  const int count = route.node_count();

  // loc_17848 / loc_17862: still on the last snapped start cell -> nothing to
  // do (the caller only resets a visual flag).
  if ((col == start_col || col == start_col2) &&
      (row == start_row || row == start_row2)) {
    out.located = false;
    out.node = node_in;
    out.penalty = 0;
    return out;
  }
  // loc_17872: off the 30x30 grid -> the -2 "off track" penalty.
  if (col < 0 || col >= Track::kSize || row < 0 || row >= Track::kSize) {
    out.located = true;
    out.node = node_in;
    out.penalty = -2;
    return out;
  }

  // detect_penalty's fixed scratch (seg001.asm's 0x385-byte / -word arrays).
  // Only `visited` needs clearing; the fork stack is written before it is read.
  std::array<uint8_t, kMaxRouteNodes> visited{};
  std::array<int, kMaxRouteNodes> stack_node{};
  std::array<int, kMaxRouteNodes> stack_dist{};
  int stack_depth = 0;  // var_21A

  int var_6 = 0;     // smallest non-zero distance seen
  int var_110 = 0;   // node that distance belongs to
  int di = 0;        // var_1E distance to the current node
  int si = node_in;

  for (;;) {
    int var_2 = route.successor(si);
    const bool seen =
        var_2 < 0 || var_2 >= count || visited[static_cast<size_t>(var_2)];
    if (seen) {
      // loc_17922: a dead end (or a closed loop) either pops a fork or ends the
      // scan. The `-1` successor guard is ours; the original would read past
      // the visited array for a track whose route has a leaf node.
      if (stack_depth > 0) {
        --stack_depth;
        si = stack_node[static_cast<size_t>(stack_depth)];
        di = stack_dist[static_cast<size_t>(stack_depth)];
        continue;
      }
      if (var_6 != 0) {
        out.located = true;
        out.node = var_110;
        out.penalty = var_6;
        return out;
      }
      // loc_17A71 / loc_17A5E: the scan lost the car; snap the start cell to it.
      start_col = start_col2 = col;
      start_row = start_row2 = row;
      out.located = true;
      out.node = node_in;
      out.penalty = -2;
      return out;
    }
    visited[static_cast<size_t>(var_2)] = 1;

    // loc_17938: does the node's tile (plus its multi-tile extent) cover the
    // car cell?
    const world::RouteNode& nd = route.node(var_2);
    const uint8_t mtf = world::track_object(nd.elem).multi_tile;
    const int row2 = nd.row + (mtf & 1);
    const int col2 = nd.col + ((mtf & 2) ? 1 : 0);
    if ((nd.col == col || col2 == col) && (nd.row == row || row2 == row)) {
      // loc_179CD: prefer reporting the predecessor when it also has a fork.
      if (route.alternate(si) != -1) var_2 = si;
      start_col = nd.col;
      start_col2 = col2;
      start_row = nd.row;
      start_row2 = row2;
      if (di <= 0) {
        out.located = true;
        out.node = var_2;
        out.penalty = di;
        return out;
      }
      if (var_6 == 0 || var_6 > di) {
        var_110 = var_2;
        var_6 = di;
      }
    }

    // loc_17A19: push the fork alternative, then advance along the primary.
    const int alt = route.alternate(si);
    if (alt != -1) {
      stack_node[static_cast<size_t>(stack_depth)] = alt;
      stack_dist[static_cast<size_t>(stack_depth)] = di;
      ++stack_depth;
    }
    if (var_2 != 0) {
      if (di != -1) ++di;
      si = var_2;
    } else {
      // Reached the start node going forward: distance wraps to -1.
      di = -1;
      si = var_2;
    }
  }
}

void RaceState::update_route(int32_t car_x_long, int32_t car_z_long) {
  if (!route.valid()) return;

  // detect_penalty col = world_x >> 10, row = 29 - (world_z >> 10); the car
  // position is 1/64 world units, so the cells are the >>16 bytes.
  const int col = static_cast<int>(car_x_long >> 16);
  const int row = Track::kSize - 1 - static_cast<int>(car_z_long >> 16);

  const PenaltyProbe probe = detect_penalty(field_2f2, col, row);
  if (!probe.located) return;  // on the start cell; the machine is skipped
  const int var_2 = probe.node;
  const int pen = probe.penalty;

  // state.c loc_172CB: classify the off-track / recovered states. Leaving the
  // off-track state clears the wrong-way run; a normal tick leaves field_45C
  // untouched (loc_172E9 skips loc_172E4 unless field_45B was 1).
  if (pen == -2) {
    field_45b = 1;
    field_45c = 0;
  } else if (field_45b == 1) {
    field_45b = 0;
    field_45c = 0;
  }
  if (field_45b != 0) {  // loc_173AD: off track / wrong way already latched
    field_2f4 = var_2;
    return;
  }

  // loc_172F3: back on route node 0 after having been elsewhere -> the car has
  // completed a lap segment; this arms the finish test.
  if (var_2 == 0 && field_2f4 != 0) {
    ++field_cd;
    apply_penalty(var_2, pen);
    return;
  }

  // loc_17308: the car is on (or just ahead of) its node -> re-locate it.
  if (pen >= 0 && pen < 3) {
    field_45c = 0;
    field_2f2 = var_2;
    field_2f4 = var_2;
    return;
  }

  // loc_17322: a distance of exactly 3 is tolerated silently.
  if (pen != -1 && pen <= 3) {
    field_2f4 = var_2;
    return;
  }

  // loc_1732E: the car has moved off the route (pen > 3), wrapped onto node 0
  // (pen == -1) or is far away. Decide whether it is driving along the route.
  //
  // NOTE: once field_45B latches to 2 the only reset is seg001.asm loc_17631,
  // inside the not-yet-ported checkpoint-arrow subsystem (sub_18D60 /
  // car_vec_unk3). Until that lands, field_45B stays 2 after real backward
  // motion; normal forward driving never reaches this branch.
  if (route.successor(field_2f4) == var_2 || route.alternate(field_2f4) == var_2) {
    ++field_45c;
  } else {
    if (route.successor(var_2) == field_2f4 || route.alternate(var_2) == field_2f4) {
      field_45b = 2;  // moving against the route -> wrong way
    }
    field_45c = 1;
  }
  if (field_45c < 3) {  // loc_17374: not three ticks yet -> keep the node
    field_2f4 = var_2;
    return;
  }
  apply_penalty(var_2, pen);
}

void RaceState::update_checkpoint(const Track& track, const CarPose& pose) {
  // state.c loc_173B3..loc_17699.
  field_45d = 0;
  const int var_1c = field_45b;  // field_45B captured at entry (loc_17402)
  if (field_45b == 1) return;    // off track: no arrow (loc_17810)
  if (!route.valid()) return;

  const fixed::Mat3 mat =
      fixed::mat_rot_zxy(pose.rot_z, pose.rot_y, pose.rot_x, 1);
  const int32_t car_x = pose.x >> 6;
  const int32_t car_y = pose.y >> 6;
  const int32_t car_z = pose.z >> 6;

  // var_matptr * (point - car_pos); the components are 16-bit words, so the
  // subtraction wraps exactly like the original's VECTOR arithmetic.
  auto rotate_delta = [&](int32_t dx, int32_t dy, int32_t dz) {
    const fixed::Vec3 v{static_cast<int16_t>(dx), static_cast<int16_t>(dy),
                        static_cast<int16_t>(dz)};
    fixed::Vec3 o;
    fixed::mat_mul_vector(v, mat, &o);
    return o;
  };

  int var_2 = 0;
  int var_3a = 0;  // var_3A: best checkpoint index within the block
  int best_z = 0;  // var_32.z: the nearest positive forward distance so far
  int si = 0;

  if (field_45b == 2) {
    // Wrong way: aim at the previous node (loc_173C2 -> loc_173F0).
    if (!pose.crashed) field_45d = 3;
    var_2 = field_2f4;
    goto walk_entry;
  }

  // field_45b == 0 (loc_173F6).
  if (car_trackdata3_index != -1) {
    if (var_1c != 0) {
      // The wrong-way latch cleared this tick: drop the stale target
      // (loc_17402 -> loc_17431).
      car_trackdata3_index = -1;
      si = 0;
      goto at_174b3;
    }
    // Keep the target only while it hangs off the current node (loc_1740F).
    if (car_trackdata3_index != field_2f2 &&
        route.successor(field_2f2) != car_trackdata3_index &&
        route.alternate(field_2f2) != car_trackdata3_index) {
      car_trackdata3_index = -1;
      si = 0;
      goto at_174b3;
    }
    {
      // loc_1743A: forward distance to the current target.
      const int32_t dy = (car_vec_y == -1) ? 0 : (car_vec_y - car_y);
      si = rotate_delta(car_vec_x - car_x, dy, car_vec_z - car_z).z;
    }
    goto at_174b3;
  }
  si = 0;

at_174b3:
  if (si >= 0x113) goto keep_point;             // target far ahead: keep it
  if (car_trackdata3_index != -1) goto advance_point;
  var_2 = field_2f2;

walk_entry:
  // loc_174C9: a fork node is not walked; keep the existing target.
  if (route.alternate(var_2) != -1) goto keep_point;
  {
    // loc_174DD: scan the block, keeping the nearest point ahead of the car.
    for (int var_2c = 0; var_2c < 64; ++var_2c) {
      world::RouteCheckpoint cp;
      const bool last = route.checkpoint(var_2, var_2c, track, &cp);
      car_vec_x = static_cast<int16_t>(cp.x);
      car_vec_y = static_cast<int16_t>(cp.y);
      car_vec_z = static_cast<int16_t>(cp.z);
      const int32_t dy = (cp.y == -1) ? -car_y : (cp.y - car_y);
      const int vz = rotate_delta(cp.x - car_x, dy, cp.z - car_z).z;
      if (var_2c == 0 || (vz < best_z && vz > 0)) {
        var_3a = var_2c;
        best_z = vz;
      }
      if (last) break;
    }
  }

  if (field_45b == 2) {
    // loc_175AF: the car may have straightened out; reset the wrong-way latch
    // (loc_17631) once its heading matches the route direction again.
    world::RouteCheckpoint p52, p1a;
    if (var_3a != 0) {
      route.checkpoint(var_2, var_3a - 1, track, &p52);
      route.checkpoint(var_2, var_3a, track, &p1a);
    } else {
      route.checkpoint(var_2, 0, track, &p52);
      route.checkpoint(var_2, 1, track, &p1a);
    }
    const int dir = fixed::polar_angle(p52.x - p1a.x, p1a.z - p52.z) & 0x3FF;
    si = (pose.rot_x - dir) & 0x3FF;
    if (si > 0x380 || si < 0x80) {
      field_45b = 0;
      field_45c = 1;
      car_trackdata3_index = var_2;
      field_ce = var_3a;
    }
    goto advance_point;  // aligned resets; otherwise step the old target
  }
  // loc_17640: normal driving adopts the node the penalty machine found.
  car_trackdata3_index = field_2f2;
  field_ce = var_3a;

advance_point:
  {
    // loc_1764C: emit the current point, then step to the next block.
    world::RouteCheckpoint cp;
    const int idx = field_ce;
    const bool last = route.checkpoint(car_trackdata3_index, idx, track, &cp);
    field_ce = idx + 1;
    if (car_trackdata3_index >= 0 && car_trackdata3_index < route.node_count()) {
      car_vec_x = static_cast<int16_t>(cp.x);
      car_vec_y = static_cast<int16_t>(cp.y);
      car_vec_z = static_cast<int16_t>(cp.z);
    }
    if (!last) goto keep_point;
    if (route.alternate(field_2f2) != -1) {
      car_trackdata3_index = -1;
    } else {
      car_trackdata3_index = route.successor(field_2f2);
    }
    field_ce = 0;
  }

keep_point:
  // loc_17699: turn the target point into the HUD arrow direction.
  if (car_trackdata3_index == -1) return;
  if (field_45b != 0) return;
  {
    const int32_t dy = (car_vec_y == -1) ? 0 : (car_vec_y - car_y);
    const fixed::Vec3 v38 = rotate_delta(car_vec_x - car_x, dy, car_vec_z - car_z);
    field_48 = fixed::polar_angle(-v38.x, v38.z) & 0x3FF;
    if (pose.crashed) return;
    // loc_17764 / loc_1776C / loc_1779E. field_B6 is never written by the
    // original, so the right arrow has no suppressor.
    const int octant = ((field_48 + 0x80) & 0x3FF) >> 8;
    if (octant == 1) {
      field_45d = 1;  // left
    } else if (octant == 3) {
      field_45d = 2;  // right
    }
  }
}

bool RaceState::update_finish(int32_t car_x_long, int32_t car_z_long,
                              const StartFinish& sf) {
  if (finished || !sf.found) return false;
  update_route(car_x_long, car_z_long);
  if (field_cd == 0) return false;
  const int dist = finish_line_distance(car_x_long, car_z_long, sf);
  if (dist < 0) {
    finished = true;
    total_finish = game_frame + penalty;  // + elapsed_time1 (0 in one run)
    return true;
  }
  return false;
}

}  // namespace s3d::game
