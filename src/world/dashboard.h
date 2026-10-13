#pragma once
// Cockpit dashboard HUD: the car-specific 2D panel drawn over the lower part of
// the in-race view (the original's default `dashb_toggle == 1` view).
//
// The player car archive STDA<carid>.PVS holds the panel artwork (seg005.asm
// setup_car_shapes 2009, loc_22D5C):
//
//   "dash"  320x70 at (0,130)  the main panel; its s2d_pos_y is `dashbmp_y`
//   "roof"  320x9  at (0,0)    the cockpit frame; its s2d_height is
//                              `roofbmpheight`
//   "whl1"/"whl2"/"whl3" 184x32 at (40,168)  steering wheel: turned left,
//                              centred, turned right (loc_2319D)
//   "ins2"  136x39 at (64,148) the speedometer/tachometer face; its top-left
//                              is the origin the needle coordinates use
//   "gbox"  64x56  at (256,144) the gear-selector plate; CARSTATE.car_knob_x/y
//                              are offsets from its top-left
//
// The companion archive STDB<carid>.PVS holds the moving parts:
//
//   "gnob"  24x23  the gear lever knob (pivot at its centre)
//   "dot"   8x4    the steering-position indicator (pivot (5,2))
//
// The original sets rect_windshield = [roofbmpheight, dashbmp_y], renders the
// 3D world inside that band and composites the panel over the rest. The
// speedometer and tachometer needles are single-colour lines
// (`meter_needle_color`, palette 15) from `spdcenter`/`revcenter` to the point
// indexed by the rev-coupled speed (`car_speed` / 640) and `car_currpm` (>> 7)
// (seg005.asm loc_23299 / loc_232BC). The digital-speed variant used by older
// cars (`spdcenter.y == 0`) and the alternate "dast"/"dasm" tachometer are not
// wired up yet.
//
// Reference: seg005.asm setup_car_shapes, loop_game and c/restunts.c run_game.

#include <cstdint>
#include <string>

#include "assets/sprite2d.h"

namespace s3d::world {

struct Dashboard {
  Sprite2D roof;      // "roof" (top strip); height = roofbmpheight
  Sprite2D dash;      // "dash" (bottom panel); pos_y = dashbmp_y
  Sprite2D wheel[3];  // "whl1"/"whl2"/"whl3" = turned-left / centred / turned-right
  Sprite2D ins2;      // "ins2" instrument face; top-left anchors the needles
  Sprite2D gear_box;  // "gbox" gear-selector plate; top-left anchors the knob
  Sprite2D gear_knob; // "gnob" gear lever knob (STDB<carid>.PVS)
  Sprite2D steer_dot; // "dot" steering-position indicator (STDB<carid>.PVS)
  bool wheels_loaded = false;
  bool instruments_loaded = false;
  bool loaded = false;

  bool valid() const { return loaded; }

  // The first valid steering-wheel sprite, or nullptr when the archive has none.
  const Sprite2D* any_wheel() const {
    for (const Sprite2D& w : wheel) {
      if (w.valid()) return &w;
    }
    return nullptr;
  }

  // roofbmpheight: the top edge of the windshield (0 when the roof is absent).
  int roof_height() const { return roof.valid() ? roof.height : 0; }
  // dashbmp_y: the bottom edge of the windshield / top of the panel. The
  // original falls back to 0xC8 (200, the full view height) without a panel.
  int dash_top() const { return dash.valid() ? dash.pos_y : 200; }
};

// Loads "dash", "roof" and the optional "whl1"/"whl2"/"whl3" steering wheel from
// <asset_dir>/STDA<car_id>.PVS (car_id is the four character id, e.g. "COUN"),
// plus the optional "ins2"/"gbox" faces from the same archive and the "gnob" /
// "dot" moving parts from STDB<car_id>.PVS. `loaded` is set only when dash and
// roof are present; everything else is best-effort (`wheels_loaded`,
// `instruments_loaded`). Returns out->loaded.
bool load_dashboard(const std::string& asset_dir, const std::string& car_id,
                    Dashboard* out);

// The shared steering value of seg005.asm loc_2319D / loc_23561:
// sign(angle) * (|angle| >> 3). The magnitude is shifted before the sign is
// reapplied, so it truncates toward zero (unlike an arithmetic shift).
int dashboard_steer_value(int16_t steering_angle);

// Index into Dashboard::wheel for `steering_angle` (CARSTATE.car_steeringAngle,
// 1/1024 turn units): 0 = "whl1" (turned left), 1 = "whl2" (centred) or 2 =
// "whl3" (turned right), from the shared steering value against a +-10 dead
// zone.
int dashboard_wheel_index(int16_t steering_angle);

// Index into a byte-pair needle-point list (SIMD "spdpoints"/"revpoints") for
// the current reading, or -1 when the list is empty:
//   speed: CARSTATE.car_speed (2^8 * mph) / 640, clamped to the last entry;
//   rev:   (unsigned) CARSTATE.car_currpm >> 7, clamped to the last entry.
int dashboard_speed_index(int32_t speed, int num_points);
int dashboard_rev_index(int16_t rpm, int num_points);

// How the original renders the speedometer, from SIMD spdcenter.y (seg005.asm
// loc_2328A): 0xFFFF (-1) means no speed needle, 0 means a digital readout, and
// any other value means an analog needle.
enum class SpeedoKind { Analog, Digital, None };
SpeedoKind dashboard_speedo_kind(int16_t center_y);

// Index into the steering-dot list (SIMD "steeringdots") for `steering_angle`:
// |steering value| clamped to the last entry, or -1 when the list is empty.
int dashboard_steer_dot_index(int16_t steering_angle, int num_points);

// Reads the (x, y) byte pair at `index` of a SIMD point list into out_x/out_y.
// The caller must pass an index within the list (e.g. from the helpers above);
// a negative index reads as (0, 0).
void dashboard_point_at(const uint8_t* points, int index, int* out_x, int* out_y);

// Screen-space rectangles for the dashboard in a view of `width` x `height`
// pixels (top-left origin). The artwork is authored for a 320x200 frame that
// the original displayed on a 4:3 screen, so its pixels are reproduced at that
// aspect (5/6 as wide as tall) with the 320-wide frame centred horizontally,
// rather than stretched to the view. `windshield_top` / `windshield_bottom`
// delimit the band the 3D world should occupy, in pixels.
struct DashboardRects {
  float roof_x;
  float roof_y;
  float roof_w;
  float roof_h;
  float dash_x;
  float dash_y;
  float dash_w;
  float dash_h;
  // The steering wheel (all three whl sprites share one authored rectangle).
  bool has_wheel;
  float wheel_x;
  float wheel_y;
  float wheel_w;
  float wheel_h;
  // The instrument face ("ins2"): its top-left is the origin the needle
  // coordinates (SIMD spdcenter/spdpoints/revcenter/revpoints) are relative to.
  bool has_instruments;
  float instrument_x;
  float instrument_y;
  float instrument_w;
  float instrument_h;
  // The gear-selector plate ("gbox"): CARSTATE.car_knob_x/y are offsets from
  // its top-left.
  bool has_gear_box;
  float gear_box_x;
  float gear_box_y;
  float gear_box_w;
  float gear_box_h;
  // The gear-lever knob ("gnob"): `knob_pivot_*` is the authored pivot that is
  // placed at (gear_box + car_knob_x/y); the knob is anchored by its pivot
  // (seg012.asm sprite_putimage_or_alt), not its top-left.
  bool has_knob;
  float knob_w;
  float knob_h;
  float knob_pivot_x;
  float knob_pivot_y;
  // The steering-position indicator ("dot"): `dot_pivot_*` is placed at the
  // authored screen coordinate from SIMD steeringdots.
  bool has_dot;
  float dot_w;
  float dot_h;
  float dot_pivot_x;
  float dot_pivot_y;
  // Authored-pixel to screen-pixel scale: x is 5/6 of y (the original 4:3
  // pixel aspect); both are view-height driven on wide views.
  float scale_x;
  float scale_y;
  float windshield_top;
  float windshield_bottom;
};

// Fills `out`. Returns false (leaving `out` untouched) when the dashboard is not
// loaded or the view has no area.
bool dashboard_rects(const Dashboard& d, int width, int height,
                     DashboardRects* out);

}  // namespace s3d::world
