#include "assets/car.h"

#include <cstring>

#include "assets/binary_reader.h"
#include "assets/res_file.h"
#include "core/log.h"

namespace s3d {

namespace {

int16_t read_i16(const uint8_t* p, size_t size, size_t off, bool* ok) {
  if (off + 2 > size) {
    *ok = false;
    return 0;
  }
  return static_cast<int16_t>(static_cast<uint16_t>(p[off]) |
                              (static_cast<uint16_t>(p[off + 1]) << 8));
}

uint16_t read_u16(const uint8_t* p, size_t size, size_t off, bool* ok) {
  return static_cast<uint16_t>(read_i16(p, size, off, ok));
}

std::string read_cstr(const std::vector<uint8_t>& v) {
  size_t n = 0;
  while (n < v.size() && v[n] != 0) ++n;
  return std::string(reinterpret_cast<const char*>(v.data()), n);
}

}  // namespace

bool CarSpec::parse_simd(const uint8_t* data, size_t size) {
  if (data == nullptr || size < 776) {
    S3D_ERROR("car: simd chunk too small (%zu bytes, expected 776)", size);
    return false;
  }
  bool ok = true;

  num_gears = data[0];
  simd_unk = data[1];
  car_mass = read_i16(data, size, 2, &ok);
  braking_eff = read_i16(data, size, 4, &ok);
  idle_rpm = read_i16(data, size, 6, &ok);
  downshift_rpm = read_i16(data, size, 8, &ok);
  upshift_rpm = read_i16(data, size, 10, &ok);
  max_rpm = read_i16(data, size, 12, &ok);
  for (int i = 0; i < 7; ++i) gear_ratios[i] = read_u16(data, size, 14 + i * 2, &ok);
  for (int i = 0; i < 7; ++i) {
    knob_points[i].x = read_i16(data, size, 28 + i * 4, &ok);
    knob_points[i].y = read_i16(data, size, 28 + i * 4 + 2, &ok);
  }

  aero_resistance = read_i16(data, size, 56, &ok);
  for (int i = 0; i < 64; ++i) {
    // (resistance * i * i) >> 9, all in 32-bit wrapping arithmetic as in the
    // original DOS binary (restunts carsetup.c aero_resistance_at).
    int32_t prod = static_cast<int32_t>(aero_resistance) * i;
    prod = prod * i;
    aero_resistance_table[i] =
        static_cast<int16_t>(static_cast<uint16_t>(prod >> 9));
  }
  idle_torque = data[58];
  std::memcpy(torque_curve, data + 59, sizeof(torque_curve));
  field_A3 = data[163];

  grip = read_i16(data, size, 164, &ok);
  for (int i = 0; i < 7; ++i) field_A6[i] = read_i16(data, size, 166 + i * 2, &ok);

  sliding = read_i16(data, size, 180, &ok);
  for (int i = 0; i < 4; ++i) surface_grip[i] = read_i16(data, size, 182 + i * 2, &ok);
  sliding_water = read_i16(data, size, 190, &ok);

  for (int i = 0; i < 2; ++i) {
    collide_points[i].x = read_i16(data, size, 200 + i * 4, &ok);
    collide_points[i].y = read_i16(data, size, 200 + i * 4 + 2, &ok);
  }
  car_height = read_i16(data, size, 208, &ok);
  for (int i = 0; i < 4; ++i) {
    wheel_coords[i].x = read_i16(data, size, 210 + i * 6, &ok);
    wheel_coords[i].y = read_i16(data, size, 210 + i * 6 + 2, &ok);
    wheel_coords[i].z = read_i16(data, size, 210 + i * 6 + 4, &ok);
  }

  // Cockpit instrument geometry (see the header): the byte-pair needle/dot
  // lists sit in the gaps between the 16-bit scalar fields.
  std::memcpy(steering_dots, data + 234, sizeof(steering_dots));
  speed_center.x = read_i16(data, size, 296, &ok);
  speed_center.y = read_i16(data, size, 298, &ok);
  speed_num_points = read_i16(data, size, 300, &ok);
  std::memcpy(speed_points, data + 302, sizeof(speed_points));
  rev_center.x = read_i16(data, size, 510, &ok);
  rev_center.y = read_i16(data, size, 512, &ok);
  rev_num_points = read_i16(data, size, 514, &ok);
  std::memcpy(rev_points, data + 516, sizeof(rev_points));

  return ok;
}

bool CarSpec::load(const std::string& path, CarSpec* out) {
  ResFile res;
  if (!res.load(path)) return false;

  const std::vector<uint8_t> simd = res.read("simd");
  if (simd.empty()) {
    S3D_ERROR("car: '%s' has no simd chunk", path.c_str());
    return false;
  }
  if (!out->parse_simd(simd.data(), simd.size())) return false;

  out->description = read_cstr(res.read("edes"));
  out->driver_name = read_cstr(res.read("gnam"));
  out->opponent_name = read_cstr(res.read("gsna"));
  return true;
}

const char* const* retail_car_files(int* count) {
  static const char* const kCars[] = {
      "CARANSX.RES", "CARAUDI.RES", "CARCOUN.RES", "CARFGTO.RES",
      "CARJAGU.RES", "CARLANC.RES", "CARLM02.RES", "CARP962.RES",
      "CARPC04.RES", "CARPMIN.RES", "CARVETT.RES",
  };
  if (count != nullptr) *count = static_cast<int>(sizeof(kCars) / sizeof(kCars[0]));
  return kCars;
}

}  // namespace s3d
