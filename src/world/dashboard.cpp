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
  S3D_INFO("dashboard: STDA%s.PVS dash %dx%d at y=%d, roof height %d", id.c_str(),
           out->dash.width, out->dash.height, out->dash.pos_y, out->roof.height);
  return true;
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
  r.windshield_top = r.roof_y + r.roof_h;
  r.windshield_bottom = std::min(r.dash_y, static_cast<float>(height));
  *out = r;
  return true;
}

}  // namespace s3d::world
