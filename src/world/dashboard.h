#pragma once
// Cockpit dashboard HUD: the car-specific 2D panel drawn over the lower part of
// the in-race view (the original's default `dashb_toggle == 1` view).
//
// The player car archive STDA<carid>.PVS holds the artwork (seg005.asm
// setup_car_shapes 2009, loc_22D5C):
//
//   "dash"  320x70 at (0,130)  the main panel; its s2d_pos_y is `dashbmp_y`
//   "roof"  320x9  at (0,0)    the cockpit frame; its s2d_height is
//                              `roofbmpheight`
//
// The original sets rect_windshield = [roofbmpheight, dashbmp_y], renders the
// 3D world inside that band and composites the panel over the rest. The steering
// wheel is chosen from three static sprites by the steering angle (loc_2319D);
// the remaining animated pieces ("gnob"/"gbox" gear lever, "ins*"/"inm*"
// instruments and, on older cars, the "dast"/"dasm" tachometer needle and mask)
// are not drawn yet.
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
  bool wheels_loaded = false;
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
// <asset_dir>/STDA<car_id>.PVS (car_id is the four character id, e.g. "COUN").
// `loaded` is set only when dash and roof are present; the wheels are best-effort
// (`wheels_loaded`). Returns out->loaded.
bool load_dashboard(const std::string& asset_dir, const std::string& car_id,
                    Dashboard* out);

// Index into Dashboard::wheel for `steering_angle` (CARSTATE.car_steeringAngle,
// 1/1024 turn units): 0 = "whl1" (turned left), 1 = "whl2" (centred) or 2 =
// "whl3" (turned right). Mirrors seg005.asm loc_2319D, which selects on
// trunc(steering_angle / 8) against the +-10 dead zone.
int dashboard_wheel_index(int16_t steering_angle);

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
  float windshield_top;
  float windshield_bottom;
};

// Fills `out`. Returns false (leaving `out` untouched) when the dashboard is not
// loaded or the view has no area.
bool dashboard_rects(const Dashboard& d, int width, int height,
                     DashboardRects* out);

}  // namespace s3d::world
