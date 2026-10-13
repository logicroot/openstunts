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
//   "ins1"/"ins3"  panel-space patches stamped over the face when the wheel is
//                  held left / right (loc_234BE / loc_234EC), with the matching
//                  "inm1"/"inm3" stencils
//   "dast"  a per-frame strip at (0,130) plus its "dasm" stencil, present on the
//           cars whose "dash" starts lower (loc_22F18)
//   "ins2"  136x39 at (64,148) the speedometer/tachometer face; its top-left
//                              is the origin the needle *and* patch coordinates
//                              use (the face is drawn into a car-sized window)
//   "gbox"  64x56  at (256,144) the gear-selector plate; CARSTATE.car_knob_x/y
//                              are offsets from its top-left
//
// The companion archive STDB<carid>.PVS holds the moving parts:
//
//   "gnob"  24x23  the gear lever knob colour (pivot at its centre); "gnab" is
//                  its AND stencil
//   "dot"   8x4    the steering-position indicator colour (its SHAPE2D pivot);
//                  "dota" is its AND stencil
//   "dig0".."dig9" 8x7 digital speedometer glyphs, loaded only for the cars
//                  whose SIMD classifies the speedometer as digital
//
// The original sets rect_windshield = [roofbmpheight, dashbmp_y], renders the
// 3D world inside that band and composites the panel over the rest. The
// speedometer and tachometer needles are single-colour lines
// (`meter_needle_color`, palette 15) from `spdcenter`/`revcenter` to the point
// indexed by the rev-coupled speed (`car_speed` / 640) and `car_currpm` (>> 7)
// (seg005.asm loc_23299 / loc_232BC); cars with a digital readout
// (`spdcenter.y == 0`) stamp glyphs at `spdpoints[0..5]` instead (loc_233A2).
//
// The "ins"/"inm", "dast"/"dasm", "gnob"/"gnab" and "dot"/"dota" pairs are
// OR-blits paired with an AND stencil: the mask clears its opaque pixels to
// black (the file stores them as palette index 0) and the shape then writes its
// own pixels, with palette index 0 acting as a no-op. Reproducing the visible
// result therefore means drawing the stencil opaquely and the shape with index 0
// treated as transparent. A mask is 0x00 over the shape's footprint and 0xFF
// everywhere else (seg012.asm sprite_putimage_and* clears, sprite_putimage_or*
// ORs), so the pair's visible result is (dst & mask) | shape.
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
  // The turned-wheel face patches: [0] = "ins1" (wheel held left), [1] = "ins3"
  // (held right). Their s2d_pos_x/y are in the *face's* coordinate space, i.e.
  // relative to ins2's top-left (the game draws them into a face-sized window).
  Sprite2D ins_patch[2];
  Sprite2D ins_mask[2];  // "inm1"/"inm3" AND stencils for ins_patch
  Sprite2D digit[10];    // "dig0".."dig9" digital speedometer glyphs (STDB)
  Sprite2D dast;         // "dast" per-frame strip (absolute screen coords)
  Sprite2D dast_mask;    // "dasm" AND stencil for dast
  Sprite2D gear_box;  // "gbox" gear-selector plate; top-left anchors the knob
  Sprite2D gear_knob; // "gnob" gear lever knob colour (STDB<carid>.PVS)
  Sprite2D gear_knob_mask;  // "gnab" AND stencil paired with gear_knob
  Sprite2D steer_dot;       // "dot" steering-position indicator colour (STDB)
  Sprite2D steer_dot_mask;  // "dota" AND stencil paired with steer_dot
  bool wheels_loaded = false;
  bool instruments_loaded = false;
  bool patches_loaded[2] = {false, false};
  bool digits_loaded = false;
  bool dast_loaded = false;
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
// plus the optional "ins2"/"gbox" faces from the same archive and the
// "gnob"/"gnab" and "dot"/"dota" moving parts (colour + AND stencil) from
// STDB<car_id>.PVS. `loaded` is set only when dash and
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

// One glyph of the digital speedometer (seg005.asm loc_2338C..loc_23433): the
// digit `value`, the SIMD spdpoints slot its top-left is drawn at (0 =
// spdpoints[0..1], 1 = [2..3], 2 = [4..5]), and whether the original stamps it
// (leading zeros are suppressed below 100/10).
struct SpeedoDigit {
  int value;
  int slot;
  bool shown;
};

// Splits `speed` (CARSTATE.car_speed, authored as 2^8 * mph) into the three
// glyphs the original shows. Always fills all three entries (hundreds, tens,
// ones) in draw order.
void dashboard_digital_digits(int32_t speed, SpeedoDigit out[3]);

// Index into the steering-dot list (SIMD "steeringdots") for `steering_angle`:
// |steering value| clamped to the last entry, or -1 when the list is empty.
int dashboard_steer_dot_index(int16_t steering_angle, int num_points);

// Reads the (x, y) byte pair at `index` of a SIMD point list into out_x/out_y.
// The caller must pass an index within the list (e.g. from the helpers above);
// a negative index reads as (0, 0).
void dashboard_point_at(const uint8_t* points, int index, int* out_x, int* out_y);

// Screen-space rectangles for the dashboard in a view of `width` x `height`
// pixels (top-left origin). The artwork is authored for a 320x200 view, so x
// scales with the view width and y with the view height: the panel always spans
// the full width and the roof caps the top edge. `windshield_top` /
// `windshield_bottom` delimit the band the 3D world should occupy, in pixels.
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
  // coordinates (SIMD spdcenter/spdpoints/revcenter/revpoints) and the
  // turned-wheel patches are relative to.
  bool has_instruments;
  float instrument_x;
  float instrument_y;
  float instrument_w;
  float instrument_h;
  // The turned-wheel face patches ("ins1"/"ins3"), anchored at the face origin
  // like the needles. [0] is the wheel-held-left patch, [1] the right one.
  bool has_patch[2];
  float patch_x[2];
  float patch_y[2];
  float patch_w[2];
  float patch_h[2];
  // The matching AND stencils ("inm1"/"inm3"): they clear to black a region that
  // can be a row taller than the patch itself, so they carry their own rects.
  bool has_patch_mask[2];
  float patch_mask_x[2];
  float patch_mask_y[2];
  float patch_mask_w[2];
  float patch_mask_h[2];
  // The per-frame "dast" strip and its "dasm" stencil: authored in absolute
  // screen coordinates (their own pos_x/pos_y, unlike the face patches),
  // stamped over the top of the panel band on the cars whose "dash" starts
  // below it.
  bool has_dast;
  float dast_x;
  float dast_y;
  float dast_w;
  float dast_h;
  bool has_dast_mask;
  float dast_mask_x;
  float dast_mask_y;
  float dast_mask_w;
  float dast_mask_h;
  // The gear-selector plate ("gbox"): CARSTATE.car_knob_x/y are offsets from
  // its top-left.
  bool has_gear_box;
  float gear_box_x;
  float gear_box_y;
  float gear_box_w;
  float gear_box_h;
  // The gear-lever knob ("gnob" + its "gnab" stencil): `knob_pivot_*` is the
  // authored pivot that is placed at (gear_box + car_knob_x/y); the knob is
  // anchored by its pivot (seg012.asm sprite_putimage_or_alt), not its top-left.
  bool has_knob;
  float knob_w;
  float knob_h;
  float knob_pivot_x;
  float knob_pivot_y;
  // The steering-position indicator ("dot" + its "dota" stencil): `dot_pivot_*`
  // is placed at the authored screen coordinate from SIMD steeringdots.
  bool has_dot;
  float dot_w;
  float dot_h;
  float dot_pivot_x;
  float dot_pivot_y;
  // x scales with the view width (width / 320), y with the height (height / 200).
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
