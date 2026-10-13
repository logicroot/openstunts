#include "world/dashboard.h"

#include <algorithm>
#include <cctype>

#include "core/log.h"
#include "core/paths.h"

namespace s3d::world {

namespace {

// The authored 320x200 reference view the dashboard coordinates are written for.
constexpr float kAuthorWidth = 320.0f;
constexpr float kAuthorHeight = 200.0f;

}  // namespace

bool load_dashboard(const std::string& asset_dir, const std::string& car_id,
                    Dashboard* out) {
  if (out == nullptr) return false;
  *out = Dashboard{};
  if (asset_dir.empty() || car_id.empty()) return false;

  std::string id = car_id;
  for (char& c : id) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  const std::string pvs = path_join(asset_dir, "STDA" + id + ".PVS");

  if (!load_sprite2d(pvs, "dash", &out->dash)) {
    S3D_WARN("dashboard: %s is missing the dash panel", pvs.c_str());
    return false;
  }
  if (!load_sprite2d(pvs, "roof", &out->roof)) {
    S3D_WARN("dashboard: %s is missing the roof", pvs.c_str());
    return false;
  }

  out->loaded = true;

  // The steering-wheel sprites are optional: older archives may lack them, and
  // each car's wheel art is self-contained ("whl1" left, "whl2" centre, "whl3"
  // right; seg005.asm locate_many_resources "whl1whl2whl3ins2gbox...").
  static const char* kWheelIds[3] = {"whl1", "whl2", "whl3"};
  int wheels = 0;
  for (int i = 0; i < 3; ++i) {
    if (load_sprite2d(pvs, kWheelIds[i], &out->wheel[i])) ++wheels;
  }
  out->wheels_loaded = (wheels == 3);
  if (wheels != 0 && wheels != 3) {
    S3D_WARN("dashboard: %s has %d/3 steering-wheel sprites", pvs.c_str(), wheels);
  }

  S3D_INFO("dashboard: STDA%s.PVS dash %dx%d at y=%d, roof height %d, %d wheel(s)",
           id.c_str(), out->dash.width, out->dash.height, out->dash.pos_y,
           out->roof.height, wheels);
  return true;
}

int dashboard_wheel_index(int16_t steering_angle) {
  // seg005.asm loc_2319D: the selection value is sign(angle) * (|angle| >> 3)
  // (magnitude shifted right, then the sign reapplied, so it truncates toward
  // zero), tested against a +-10 dead zone: < -10 -> whl1, [-10,10] -> whl2,
  // > 10 -> whl3.
  int magnitude = static_cast<int>(steering_angle);
  const bool negative = magnitude < 0;
  if (negative) magnitude = -magnitude;
  int value = magnitude >> 3;
  if (negative) value = -value;
  if (value < -10) return 0;
  if (value > 10) return 2;
  return 1;
}

bool dashboard_rects(const Dashboard& d, int width, int height,
                     DashboardRects* out) {
  if (!d.valid() || out == nullptr || width <= 0 || height <= 0) return false;
  const float sx = static_cast<float>(width) / kAuthorWidth;
  const float sy = static_cast<float>(height) / kAuthorHeight;

  DashboardRects r;
  r.roof_x = static_cast<float>(d.roof.pos_x) * sx;
  r.roof_y = static_cast<float>(d.roof.pos_y) * sy;
  r.roof_w = static_cast<float>(d.roof.width) * sx;
  r.roof_h = static_cast<float>(d.roof.height) * sy;
  r.dash_x = static_cast<float>(d.dash.pos_x) * sx;
  r.dash_y = static_cast<float>(d.dash.pos_y) * sy;
  r.dash_w = static_cast<float>(d.dash.width) * sx;
  r.dash_h = static_cast<float>(d.dash.height) * sy;
  const Sprite2D* wheel = d.any_wheel();
  r.has_wheel = wheel != nullptr;
  r.wheel_x = wheel ? static_cast<float>(wheel->pos_x) * sx : 0.0f;
  r.wheel_y = wheel ? static_cast<float>(wheel->pos_y) * sy : 0.0f;
  r.wheel_w = wheel ? static_cast<float>(wheel->width) * sx : 0.0f;
  r.wheel_h = wheel ? static_cast<float>(wheel->height) * sy : 0.0f;
  r.windshield_top = r.roof_y + r.roof_h;
  r.windshield_bottom = std::min(r.dash_y, static_cast<float>(height));
  *out = r;
  return true;
}

}  // namespace s3d::world
