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

  // The instrument face and gear-selector plate come from the same archive
  // (loc_22D5C: locate_many_resources "...ins2gbox...").
  const bool have_ins2 = load_sprite2d(pvs, "ins2", &out->ins2);
  const bool have_gbox = load_sprite2d(pvs, "gbox", &out->gear_box);
  if (!have_ins2) S3D_WARN("dashboard: %s is missing the ins2 instrument face", pvs.c_str());
  if (!have_gbox) S3D_WARN("dashboard: %s is missing the gbox gear-selector plate", pvs.c_str());

  // The turned-wheel face patches and their stencils come from the same archive
  // (loc_234BE/loc_234EC: "ins1"/"ins3" masked by "inm1"/"inm3"). They are in
  // the instrument face's coordinate space, so they need ins2 to place them.
  static const char* kPatchIds[2] = {"ins1", "ins3"};
  static const char* kPatchMaskIds[2] = {"inm1", "inm3"};
  for (int i = 0; i < 2; ++i) {
    const bool have_patch = load_sprite2d(pvs, kPatchIds[i], &out->ins_patch[i]);
    const bool have_mask = load_sprite2d(pvs, kPatchMaskIds[i], &out->ins_mask[i]);
    out->patches_loaded[i] = have_patch && have_mask;
    if (!out->patches_loaded[i]) {
      S3D_WARN("dashboard: %s is missing the %s/%s wheel patch", pvs.c_str(),
               kPatchIds[i], kPatchMaskIds[i]);
    }
  }

  // The per-frame "dast" strip and its "dasm" stencil (loc_22F18): only the cars
  // whose "dash" starts below y=130 split the panel this way.
  const bool have_dast = load_sprite2d(pvs, "dast", &out->dast);
  const bool have_dasm = load_sprite2d(pvs, "dasm", &out->dast_mask);
  out->dast_loaded = have_dast && have_dasm;

  // The moving parts (gear lever knob and steering dot) live in STDB<carid>.PVS
  // (loc_22E10: locate_many_resources "gnobgnabdotdotadot1dot2").
  const std::string pvs_b = path_join(asset_dir, "STDB" + id + ".PVS");
  const bool have_knob = load_sprite2d(pvs_b, "gnob", &out->gear_knob);
  // The steering-dot resource id is four characters, "dot " with a trailing
  // space (seg005.asm loc_22E10 names "gnobgnabdot dotadot1dot2").
  const bool have_dot = load_sprite2d(pvs_b, "dot ", &out->steer_dot);
  if (!have_knob) S3D_WARN("dashboard: %s is missing the gnob gear lever", pvs_b.c_str());
  if (!have_dot) S3D_WARN("dashboard: %s is missing the dot indicator", pvs_b.c_str());

  // The digital speedometer glyphs also come from STDB (loc_2105:
  // locate_many_resources "dig0dig1...dig9"). Only the cars whose SIMD marks the
  // speedometer digital carry them.
  int digits = 0;
  for (int i = 0; i < 10; ++i) {
    const std::string digit_id = "dig" + std::to_string(i);
    if (load_sprite2d(pvs_b, digit_id.c_str(), &out->digit[i])) ++digits;
  }
  out->digits_loaded = (digits == 10);

  // `instruments_loaded` gates the needle overlay: the needle geometry is a
  // property of the car's SIMD data, so the origin (`ins2`) alone decides.
  out->instruments_loaded = have_ins2;

  S3D_INFO("dashboard: STDA%s.PVS dash %dx%d at y=%d, roof height %d, %d wheel(s), "
           "ins2 %d, gbox %d, gnob %d, dot %d, patch %d/%d, digits %d, dast %d",
           id.c_str(), out->dash.width, out->dash.height, out->dash.pos_y,
           out->roof.height, wheels, have_ins2 ? 1 : 0, have_gbox ? 1 : 0,
           have_knob ? 1 : 0, have_dot ? 1 : 0,
           out->patches_loaded[0] ? 1 : 0, out->patches_loaded[1] ? 1 : 0,
           digits, out->dast_loaded ? 1 : 0);
  return true;
}

int dashboard_steer_value(int16_t steering_angle) {
  // seg005.asm loc_2319D / loc_23561: the selection value is
  // sign(angle) * (|angle| >> 3) (magnitude shifted right, then the sign
  // reapplied, so it truncates toward zero).
  int magnitude = static_cast<int>(steering_angle);
  const bool negative = magnitude < 0;
  if (negative) magnitude = -magnitude;
  int value = magnitude >> 3;
  if (negative) value = -value;
  return value;
}

int dashboard_wheel_index(int16_t steering_angle) {
  // The shared steering value tested against a +-10 dead zone:
  // < -10 -> whl1, [-10,10] -> whl2, > 10 -> whl3.
  const int value = dashboard_steer_value(steering_angle);
  if (value < -10) return 0;
  if (value > 10) return 2;
  return 1;
}

int dashboard_speed_index(int32_t speed, int num_points) {
  if (num_points <= 0) return -1;
  // loc_23299: ax = car_speed, dx = 0, cx = 640, div cx (unsigned 32/16).
  int32_t index = speed / 640;
  if (index >= num_points) index = num_points - 1;
  if (index < 0) index = 0;
  return static_cast<int>(index);
}

int dashboard_rev_index(int16_t rpm, int num_points) {
  if (num_points <= 0) return -1;
  // loc_232BC: ax = car_currpm, cl = 7, shr ax, cl (logical 16-bit shift).
  int32_t index = static_cast<uint16_t>(rpm) >> 7;
  if (index >= num_points) index = num_points - 1;
  if (index < 0) index = 0;
  return static_cast<int>(index);
}

SpeedoKind dashboard_speedo_kind(int16_t center_y) {
  // loc_2328A: 0xFFFF -> no needle, 0 -> digital, anything else -> analog.
  if (center_y == 0) return SpeedoKind::Digital;
  if (center_y == -1) return SpeedoKind::None;
  return SpeedoKind::Analog;
}

void dashboard_digital_digits(int32_t speed, SpeedoDigit out[3]) {
  if (out == nullptr) return;
  // loc_233A2: si = car_speed >> 8 (a 16-bit logical shift of the low word).
  int si = static_cast<int>(static_cast<uint16_t>(speed & 0xFFFF) >> 8);
  // loc_2338C: the hundreds glyph, then subtract it from the reading.
  int hundreds = 0;
  if (si >= 200) {
    hundreds = 2;
    si -= 200;
  } else if (si >= 100) {
    hundreds = 1;
    si -= 100;
  }
  bool drawn = false;
  out[0].slot = 0;
  out[0].value = hundreds;
  out[0].shown = (hundreds != 0);
  if (out[0].shown) drawn = true;
  // loc_233EA: the tens glyph, suppressed only while nothing has been drawn.
  const int tens = si / 10;
  out[1].slot = 1;
  out[1].value = tens;
  out[1].shown = (tens != 0) || drawn;
  if (out[1].shown) {
    si -= tens * 10;
  }
  // loc_23433: the ones glyph is always stamped.
  out[2].slot = 2;
  out[2].value = si;
  out[2].shown = true;
}

int dashboard_steer_dot_index(int16_t steering_angle, int num_points) {
  if (num_points <= 0) return -1;
  // loc_235C5: the dot index is |steering value|.
  int index = dashboard_steer_value(steering_angle);
  if (index < 0) index = -index;
  if (index >= num_points) index = num_points - 1;
  return index;
}

void dashboard_point_at(const uint8_t* points, int index, int* out_x, int* out_y) {
  int x = 0, y = 0;
  if (points != nullptr && index >= 0) {
    x = points[index * 2];
    y = points[index * 2 + 1];
  }
  if (out_x != nullptr) *out_x = x;
  if (out_y != nullptr) *out_y = y;
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

  r.has_instruments = d.ins2.valid();
  r.instrument_x = static_cast<float>(d.ins2.pos_x) * sx;
  r.instrument_y = static_cast<float>(d.ins2.pos_y) * sy;
  r.instrument_w = static_cast<float>(d.ins2.width) * sx;
  r.instrument_h = static_cast<float>(d.ins2.height) * sy;

  // The turned-wheel face patches are anchored at the instrument face's origin:
  // the original stamps them into the face-sized window (loc_234BE/loc_234EC).
  for (int i = 0; i < 2; ++i) {
    const Sprite2D& p = d.ins_patch[i];
    const Sprite2D& m = d.ins_mask[i];
    r.has_patch[i] = d.patches_loaded[i] && p.valid();
    r.patch_x[i] = static_cast<float>(d.ins2.pos_x + p.pos_x) * sx;
    r.patch_y[i] = static_cast<float>(d.ins2.pos_y + p.pos_y) * sy;
    r.patch_w[i] = static_cast<float>(p.width) * sx;
    r.patch_h[i] = static_cast<float>(p.height) * sy;
    r.has_patch_mask[i] = d.patches_loaded[i] && m.valid();
    r.patch_mask_x[i] = static_cast<float>(d.ins2.pos_x + m.pos_x) * sx;
    r.patch_mask_y[i] = static_cast<float>(d.ins2.pos_y + m.pos_y) * sy;
    r.patch_mask_w[i] = static_cast<float>(m.width) * sx;
    r.patch_mask_h[i] = static_cast<float>(m.height) * sy;
  }

  // The per-frame "dast" strip is authored in absolute screen coordinates.
  r.has_dast = d.dast_loaded && d.dast.valid();
  r.dast_x = static_cast<float>(d.dast.pos_x) * sx;
  r.dast_y = static_cast<float>(d.dast.pos_y) * sy;
  r.dast_w = static_cast<float>(d.dast.width) * sx;
  r.dast_h = static_cast<float>(d.dast.height) * sy;
  r.has_dast_mask = d.dast_loaded && d.dast_mask.valid();
  r.dast_mask_x = static_cast<float>(d.dast_mask.pos_x) * sx;
  r.dast_mask_y = static_cast<float>(d.dast_mask.pos_y) * sy;
  r.dast_mask_w = static_cast<float>(d.dast_mask.width) * sx;
  r.dast_mask_h = static_cast<float>(d.dast_mask.height) * sy;

  r.has_gear_box = d.gear_box.valid();
  r.gear_box_x = static_cast<float>(d.gear_box.pos_x) * sx;
  r.gear_box_y = static_cast<float>(d.gear_box.pos_y) * sy;
  r.gear_box_w = static_cast<float>(d.gear_box.width) * sx;
  r.gear_box_h = static_cast<float>(d.gear_box.height) * sy;

  r.has_knob = d.gear_knob.valid();
  r.knob_w = static_cast<float>(d.gear_knob.width) * sx;
  r.knob_h = static_cast<float>(d.gear_knob.height) * sy;
  r.knob_pivot_x = static_cast<float>(d.gear_knob.pivot_x);
  r.knob_pivot_y = static_cast<float>(d.gear_knob.pivot_y);

  r.has_dot = d.steer_dot.valid();
  r.dot_w = static_cast<float>(d.steer_dot.width) * sx;
  r.dot_h = static_cast<float>(d.steer_dot.height) * sy;
  r.dot_pivot_x = static_cast<float>(d.steer_dot.pivot_x);
  r.dot_pivot_y = static_cast<float>(d.steer_dot.pivot_y);

  r.scale_x = sx;
  r.scale_y = sy;
  r.windshield_top = r.roof_y + r.roof_h;
  r.windshield_bottom = std::min(r.dash_y, static_cast<float>(height));
  *out = r;
  return true;
}

}  // namespace s3d::world
