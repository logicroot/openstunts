#include "assets/opponent.h"

#include <vector>

#include "assets/compression.h"
#include "assets/res_file.h"
#include "core/log.h"

namespace s3d {

namespace {

std::string read_cstr(const std::vector<uint8_t>& v) {
  size_t n = 0;
  while (n < v.size() && v[n] != 0) ++n;
  return std::string(reinterpret_cast<const char*>(v.data()), n);
}

}  // namespace

std::string opponent_file(int index) {
  if (index < 1) index = 1;
  if (index > OpponentData::kOpponentCount) index = OpponentData::kOpponentCount;
  return "OPP" + std::to_string(index) + ".PRE";
}

bool OpponentData::load(const std::string& pre_path) {
  std::vector<uint8_t> raw;
  if (!decompress_file(pre_path, &raw)) {
    S3D_ERROR("opponent: cannot decompress '%s'", pre_path.c_str());
    return false;
  }
  ResFile res;
  if (!res.load(raw.data(), raw.size())) {
    S3D_ERROR("opponent: '%s' is not a resource archive", pre_path.c_str());
    return false;
  }

  const std::vector<uint8_t> sped = res.read("sped");
  if (sped.size() < static_cast<size_t>(kSpeedCount)) {
    S3D_ERROR("opponent: '%s' has no sped table", pre_path.c_str());
    return false;
  }
  for (int i = 0; i < kSpeedCount; ++i) sped_[i] = sped[static_cast<size_t>(i)];

  description_ = read_cstr(res.read("edes"));
  name_ = read_cstr(res.read("enam"));
  return true;
}

uint8_t OpponentData::speed_for(uint8_t speed_code, uint8_t surface) const {
  const int index = static_cast<int>(speed_code) + static_cast<int>(surface);
  // The original indexes a 16-byte table with no bounds check; the elements
  // whose speed code plus surface reaches 16/17 read into the following dseg
  // bytes (zero in practice). Return 0 rather than run past the table.
  if (index < 0 || index >= kSpeedCount) return 0;
  return sped_[static_cast<size_t>(index)];
}

uint8_t OpponentData::engine_torque() const {
  // seg001.asm:6022:  al = oppnentSped[0]; ax = 0xC8 - al; ax = -ax; ax >>= 1.
  const int value = (0xC8 - static_cast<int>(sped_[0])) >> 1;
  return static_cast<uint8_t>(value & 0xFF);
}

}  // namespace s3d
