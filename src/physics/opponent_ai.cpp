// Opponent car AI, a literal port of seg001.asm `opponent_op` (lines 77-811).
// See opponent_ai.h for the behavioural overview; every jump target below is
// annotated with the original label.

#include "physics/opponent_ai.h"

#include <cstdint>

#include "assets/car.h"
#include "assets/opponent.h"
#include "physics/car_engine.h"
#include "physics/car_motion.h"
#include "physics/car_steering.h"
#include "physics/fixed_math.h"
#include "physics/wrap_math.h"
#include "assets/trk.h"
#include "world/track_route.h"

namespace s3d::physics {

using fixed::Mat3;
using fixed::Vec3;
using world::RouteCheckpoint;

namespace {

// (pos >> 6) exactly as the original's 32-bit `sar dx,1 / rcr ax,1` x 6 loop
// truncates a long into a word (car_posWorld1.lx >> 6).
inline int16_t shr6(int32_t v) { return static_cast<int16_t>(v >> 6); }

// trackcenterpos[row] = row * 0x400 + 0x200 (the start-cell row centre).
inline int16_t row_center(int row) { return static_cast<int16_t>(row * 0x400 + 0x200); }

// trackcenterpos2[col] = col * 0x400 + 0x200 (the start-cell column centre).
inline int16_t col_center(int col) { return static_cast<int16_t>(col * 0x400 + 0x200); }

// (a + b) >> 1 with the original's 32-bit signed halving (sub_18D60 loc_18FCF).
inline int16_t midpoint(int16_t a, int16_t b) {
  return static_cast<int16_t>((static_cast<int32_t>(a) + static_cast<int32_t>(b)) >> 1);
}

// Sub_18D60 stores the first/second course points in car_vec_unk4/unk5 and the
// 0xFFFF y sentinel (two arrows, no height) verbatim.
inline int16_t edge_y(const int16_t midpoint_y, const int16_t edge_y) {
  return midpoint_y == static_cast<int16_t>(0xFFFF) ? static_cast<int16_t>(0xFFFF)
                                                    : midpoint(edge_y, midpoint_y);
}

// Writes one RouteCheckpoint into the car's route fields exactly where
// sub_18D60 put them: arg_2 = &car_vec_unk3 (midpoint), +6 = first point
// (+0xC = second point) and +0x12 = the reverse-path word.
void write_checkpoint(CarState* opp, const RouteCheckpoint& cp) {
  opp->car_route_target.x = static_cast<int16_t>(cp.x);
  opp->car_route_target.y = static_cast<int16_t>(cp.y);
  opp->car_route_target.z = static_cast<int16_t>(cp.z);
  opp->car_route_first_edge.x = static_cast<int16_t>(cp.ax);
  opp->car_route_first_edge.y = static_cast<int16_t>(cp.ay);
  opp->car_route_first_edge.z = static_cast<int16_t>(cp.az);
  opp->car_route_second_edge.x = static_cast<int16_t>(cp.bx);
  opp->car_route_second_edge.y = static_cast<int16_t>(cp.by);
  opp->car_route_second_edge.z = static_cast<int16_t>(cp.bz);
  opp->car_route_has_reverse_path = static_cast<int16_t>(cp.opp1);
}

// TrackRoute::checkpoint returns *true only for the last arrow* of a block
// (sub_18D60's return), so a valid-but-not-last call also returns false. The
// one case where cp is not written at all is an index past the block's arrow
// count (the original reads junk camera data there), which leaves RouteCheckpoint
// at its default zeros. A real checkpoint's midpoints never sit at the world
// origin, so an untouched midpoint cleanly marks "nothing written"; turning
// such a corrupted advance into "keep the previous checkpoint" is a deliberate
// hardening of the original behaviour.
bool checkpoint_wrote(const RouteCheckpoint& cp) { return cp.x != 0 || cp.z != 0; }

// Indexes `path` at `index`, clamping to the array for safety (a corrupted
// state in the original reads past the array instead).
uint16_t path_node(const std::vector<uint16_t>& path, int index) {
  if (path.empty()) return 0;
  if (index < 0) return path.front();
  if (index >= static_cast<int>(path.size())) return path.back();
  return path[static_cast<size_t>(index)];
}

// The shared advance of opponent_op (loc_14866 and the inline copy at
// loc_14AAE): the sub_18D60 checkpoint call plus its return bookkeeping
//
//   last = checkpoint(node, field_CE); field_CE++;
//   if (last) {
//     index++;
//     if (trackdata3[index] == 0) { field_CD++; index = 0; }
//     field_CE = 0;
//   }
//
// The course path `trackdata3` ends with the 0x0000 wrap marker, so "the next
// node is 0" is exactly the lap wrap.
void advance_checkpoint(const OpponentRaceSetup& setup, OpponentAiState* ai, CarState* opp) {
  const int index = opp->car_route_index;
  const int ce = opp->car_route_point_index;
  ++opp->car_route_point_index;

  RouteCheckpoint cp;
  const bool last = setup.route->checkpoint(path_node(setup.path, index), ce, *setup.track, &cp);
  if (checkpoint_wrote(cp)) {  // see the helper: valid arrows write, garbage does not
    write_checkpoint(opp, cp);
    ai->target_speed_code = setup.data->speed_for(cp.speed_code, cp.surface);
  }

  if (last) {
    ++opp->car_route_index;
    if (path_node(setup.path, opp->car_route_index) == 0) {
      ++opp->car_lap_count;
      opp->car_route_index = 0;
    }
    opp->car_route_point_index = 0;
  }
}

}  // namespace

void init_opponent_checkpoint(const OpponentRaceSetup& setup, OpponentAiState* ai,
                              CarState* opp) {
  // init_game_state's opponent block (restunts.c:571-577): sub_18D60 of path
  // node 0 at field_CE == 0 writes the first course checkpoint and field_3F9;
  // the post-call field_CE++ leaves the car at course arrow 1.
  RouteCheckpoint cp;
  setup.route->checkpoint(path_node(setup.path, 0), 0, *setup.track, &cp);
  if (checkpoint_wrote(cp)) {
    write_checkpoint(opp, cp);
    ai->target_speed_code = setup.data->speed_for(cp.speed_code, cp.surface);
  }
  opp->car_route_point_index = 1;
}

uint8_t run_opponent_ai(const OpponentRaceSetup& setup, OpponentAiState* ai,
                        CarState* opp, const CarState& player, const CarSpec& opp_spec,
                        int frames_per_sec, const GroundSampler& ground,
                        const TrackCollision* collision) {
  uint8_t field_45e = 0;

  // var_14 / var_10: the frame-rate steering and spinning-wheel steps.
  int var_14;
  int var_10;
  if (frames_per_sec == kGameFrameRateNormal) {  // line 115: == 20 fps
    var_14 = 8;
    var_10 = 1;
  } else {  // loc_1472E
    var_14 = 0x10;
    var_10 = 2;
  }

  // var_20: may still clamp, cleared while a wheel is spinning and on the
  // menu autopilot (loc_14746); while cleared, out-of-envelope steering
  // advance the course instead of clamping to +-0x41.
  int var_20 = (opp->car_velocity_heading_offset != 0 || setup.game_inputmode == 2) ? 1 : 0;

  // Word coordinates of both cars (loc_14750).
  const int16_t var_28 = shr6(opp->car_position.x);
  const int16_t var_2A = shr6(opp->car_position.y);
  const int16_t var_36 = shr6(opp->car_position.z);
  const int16_t var_34 = shr6(player.car_position.x);
  const int16_t var_3E = shr6(player.car_position.y);
  const int16_t var_40 = shr6(player.car_position.z);

  // Steering navigation matrix (field_CF toggles around it).
  opp->car_route_flag = 0;
  Mat3 nav = fixed::mat_rot_zxy(opp->car_rotate.z, opp->car_rotate.y, opp->car_rotate.x, 1);
  opp->car_route_flag = 1;

  // Reusable locals (the [bp-...] words).
  int16_t var_2C = 0;
  int16_t var_3C = 0, var_3A = 0, var_38 = 0;
  int16_t var_26 = 0, var_24 = 0, var_22 = 0;
  int16_t var_C = 0, var_A = 0, var_8 = 0;
  int16_t var_1C = 0, var_1A = 0, var_18 = 0;
  int16_t var_6 = 0;
  int var_E = 0;

  // Crashed: no steering/navigation at all, just the braking tail below.
  if (opp->car_crashBmpFlag != kCrashEventNone) {
    if (opp->car_actual_speed == kSpeedStopped) opp->car_route_flag = 0;  // loc_14803
    goto var_e_select;                                                    // loc_14B72
  }

  // loc_1480C: the (fake-3D) distance to the current checkpoint midpoint.
  var_3C = opp->car_route_target.x;
  var_3A = opp->car_route_target.y;
  var_38 = opp->car_route_target.z;
  int16_t radius;
  if (var_3A == static_cast<int16_t>(0xFFFF)) {
    radius = static_cast<int16_t>(
        fixed::polar_radius2d(wrap_sub(var_3C, var_28), wrap_sub(var_38, var_36)));
  } else {
    radius = static_cast<int16_t>(fixed::polar_radius3d(Vec3{
        wrap_sub(var_3C, var_28), wrap_sub(var_3A, var_2A), wrap_sub(var_38, var_36)}));
  }
  if (radius >= 0xC8) goto checkpoint_present;  // loc_1485E -> loc_148B3

advance:  // loc_14866 (also reached from the steering retry, loc_14B0F)
  advance_checkpoint(setup, ai, opp);
  // fall through

checkpoint_present:  // loc_148B3
  if (setup.game_inputmode == 2) goto aim_direct;  // loc_148BA

  // loc_148EC: rotate the player-relative vector into the car frame.
  var_26 = wrap_sub(var_34, var_28);
  var_24 = wrap_sub(var_3E, var_2A);
  var_22 = wrap_sub(var_40, var_36);
  {
    Vec3 out;
    fixed::mat_mul_vector(Vec3{var_26, var_24, var_22}, nav, &out);
    var_1C = out.x;
    var_1A = out.y;
    var_18 = out.z;
  }
  if (var_1A > 0x5A) goto aim_direct;
  if (absolute16(var_1C) > 0xB4) goto aim_direct;
  if (var_18 > 0x258) goto aim_direct;
  if (var_18 < static_cast<int16_t>(0xFF4C)) goto aim_direct;

  // loc_1494A: the player is within 90/180/600 units of the route mid-line
  // (plus 0xFF4C behind), so aim at the midpoint of the course edge on the
  // player's side instead of the middle of the road.
  var_26 = wrap_sub(var_34, opp->car_route_target.x);
  if (opp->car_route_target.y == static_cast<int16_t>(0xFFFF)) {
    var_24 = 0;
  } else {
    var_24 = wrap_sub(var_3E, opp->car_route_target.y);  // loc_14962
  }
  var_22 = wrap_sub(var_40, opp->car_route_target.z);
  {
    Vec3 rel;
    fixed::mat_mul_vector(Vec3{var_26, var_24, var_22}, nav, &rel);
    var_6 = rel.x;  // var_4 / var_2 unused beyond the side test
  }
  if (var_6 >= 0) goto intercept_first_edge;  // loc_149FE

  // The player sits on the second-edge side (var_6 < 0).
  var_C = midpoint(opp->car_route_second_edge.x, opp->car_route_target.x);
  var_A = edge_y(opp->car_route_target.y, opp->car_route_second_edge.y);
  var_8 = midpoint(opp->car_route_second_edge.z, opp->car_route_target.z);
  if (var_18 > static_cast<int16_t>(0xFFB2) && player.car_crashBmpFlag == kCrashEventNone)
    field_45e = 2;  // loc_149F5
  goto aim;          // loc_148C7

intercept_first_edge:  // loc_149FE
  var_C = midpoint(opp->car_route_first_edge.x, opp->car_route_target.x);
  var_A = edge_y(opp->car_route_target.y, opp->car_route_first_edge.y);
  var_8 = midpoint(opp->car_route_first_edge.z, opp->car_route_target.z);
  if (var_18 > static_cast<int16_t>(0xFFB2) && player.car_crashBmpFlag == kCrashEventNone)
    field_45e = 1;  // loc_14A65
  goto aim;          // loc_148C7

aim_direct:  // loc_148BA
  var_C = opp->car_route_target.x;
  var_A = opp->car_route_target.y;
  var_8 = opp->car_route_target.z;

aim:  // loc_148C7
  var_26 = var_C;
  var_24 = var_A;
  var_22 = var_8;
  var_26 = wrap_sub(var_26, var_28);
  if (var_A == static_cast<int16_t>(0xFFFF)) {
    var_24 = 0;  // loc_148E3
  } else {
    var_24 = wrap_sub(var_24, var_2A);  // loc_14A6E
  }
  var_22 = wrap_sub(var_22, var_36);  // loc_14A74
  {
    Vec3 out;
    fixed::mat_mul_vector(Vec3{var_26, var_24, var_22}, nav, &out);
    var_3C = out.x;
    var_3A = out.y;
    var_38 = out.z;
  }
  var_2C = static_cast<int16_t>(fixed::polar_angle(var_3C, var_38));

  // loc_14AAE: a turn sharper than 0x100 while the wheels are on the ground
  // immediately advances to the next course checkpoint.
  if (opp->car_slidingFlag == kCarSlidingInactive && absolute16(var_2C) > 0x100) {
    advance_checkpoint(setup, ai, opp);  // the inline copy at 511-537
  }

// loc_14B03: clamp the requested steering envelope.
  if (var_2C > 0x41) {
    if (var_20 == 0) {  // loc_14B0F
      var_20 = 1;
      goto advance;  // jmp loc_14866 (recompute with the next checkpoint)
    }
    var_2C = 0x41;  // loc_14B16
  } else if (var_2C < static_cast<int16_t>(0xFFBF)) {
    if (var_20 == 0) {  // loc_14B0F
      var_20 = 1;
      goto advance;
    }
    var_2C = static_cast<int16_t>(0xFFBF);
  }
  if (opp->car_sumSurfFrontWheels == kWheelContactNone) var_2C = 0;  // loc_14B2F
  {
    const int16_t diff = wrap_sub(var_2C, opp->car_steeringAngle);  // loc_14B3B
    if (absolute16(diff) <= var_14) {
      opp->car_steeringAngle = var_2C;  // loc_14B6C: close enough, snap
    } else if (var_2C >= opp->car_steeringAngle) {
      opp->car_steeringAngle =
          wrap_add(opp->car_steeringAngle, static_cast<int16_t>(var_14));  // loc_14B62
    } else {
      opp->car_steeringAngle =
          wrap_sub(opp->car_steeringAngle, static_cast<int16_t>(var_14));
    }
  }

var_e_select:  // loc_14B72
  var_E = 0;
  if (opp->car_sumSurfRearWheels != kWheelContactNone) {
    if (opp->car_crashBmpFlag != kCrashEventNone) {
      var_E = 2;  // loc_14BF6: brake while crashed
    } else if (opp->car_velocity_heading_offset != 0) {
      // A spinning wheel sheds car_speed2 by var_10 * 0x200 per frame.
      const uint16_t spin = static_cast<uint16_t>(var_10 << 9);
      if (spin <= opp->car_actual_speed) {
        opp->car_actual_speed = static_cast<uint16_t>(opp->car_actual_speed - spin);
      } else {
        opp->car_actual_speed = kSpeedStopped;  // line 600: stall the spin
        opp->car_velocity_heading_offset = 0;
      }
    } else {  // loc_14BB6
      if (opp->car_demandedGrip > opp->car_surfacegrip_sum) {
        var_E = 2;  // loc_14BF6
      } else {
        const uint16_t var_12 = setup.game_inputmode == 2
                                    ? static_cast<uint16_t>(0x4000)  // autopilot rpm
                                    : static_cast<uint16_t>(ai->target_speed_code << 8);
        const uint16_t lo = static_cast<uint16_t>(var_12 - 0x100);  // accel band
        if (lo > opp->car_rev_speed) {
          var_E = 1;  // accelerate
        } else {
          const uint16_t hi = static_cast<uint16_t>(var_12 + 0x300);  // brake band
          if (hi < opp->car_rev_speed) var_E = 2;                     // loc_14BF6
        }
      }
    }
  }

  // loc_14BFA: the physics tail for this car. `opponent_target_speed` receives
  // the raw sped[0] byte so that update_car_speed's (200 - target) >> 1 drag
  // reproduces the asm's var_currTorque = (0xC8 - oppnentSped[0]) >> 1.
  update_car_speed(static_cast<uint8_t>(var_E), opp_spec, opp, frames_per_sec,
                   /*is_opponent=*/true, setup.data->speed_table()[0], nullptr);
  update_grip(opp, opp_spec, nullptr, kGripBehaviorOpponent);
  step_car_motion(opp, opp_spec, frames_per_sec, ground, collision);

  // The loc_16840 latch clear and the car-vs-car collision live in the drive
  // scene (car_car_resolve), which runs this module's pass and then the
  // opponent's pass, matching the original's player_op -> opponent_op order.

  // loc_14C49: the checkpoint heading error, skipped while crashed.
  if (opp->car_crashBmpFlag != kCrashEventNone) goto finish_line;
  {
    var_26 = wrap_sub(opp->car_route_target.x, shr6(opp->car_position.x));
    var_24 = wrap_sub(opp->car_route_target.y, shr6(opp->car_position.y));
    var_22 = wrap_sub(opp->car_route_target.z, shr6(opp->car_position.z));
    nav = fixed::mat_rot_zxy(opp->car_rotate.z, opp->car_rotate.y, opp->car_rotate.x, 1);
    Vec3 out;
    fixed::mat_mul_vector(Vec3{var_26, var_24, var_22}, nav, &out);
    opp->car_route_heading_error =
        static_cast<int16_t>(fixed::polar_angle(wrap_negate(out.x), out.z)) & 0x3FF;
  }

finish_line:  // loc_14CD7
  if (opp->car_lap_count != 0) {
    // Distance from the start cell centre along the start direction; once it
    // goes negative the lap-1+ car has crossed the line and finishes.
    const int16_t band =
        wrap_add(fixed::multiply_and_scale(
                     fixed::cos_fast(static_cast<uint16_t>(setup.track_angle)),
                     wrap_sub(row_center(setup.start_row), shr6(opp->car_position.z))),
                 fixed::multiply_and_scale(
                     fixed::sin_fast(static_cast<uint16_t>(setup.track_angle)),
                     wrap_sub(col_center(setup.start_col), shr6(opp->car_position.x))));
    if (band < 0) opp->car_crashBmpFlag = kCrashEventFinish;  // update_crash_state(3, 1)
  }

  return field_45e;
}

}  // namespace s3d::physics