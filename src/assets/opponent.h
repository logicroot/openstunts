#pragma once
// Opponent definition, loaded from OPPn.PRE (1..6), a *compressed* resource
// archive (decompress_file, then ResFile::load).
//
// The physics only needs the `sped` chunk (16 bytes), which seg004.asm copies
// into `oppnentSped[16]` during track setup. It is read in two places:
//
//   * seg001.asm sub_18D60  ->  oppnentSped[si_oppSpedCode + ss_surfaceType]
//     stored into state.field_3F9 (the opponent's per-element speed limit);
//   * seg001.asm update_car_speed (MplayerFlag != 0) ->
//     currTorque = ((0xC8 - oppnentSped[0]) >> 1) & 0xFF, the opponent-only
//     engine torque / top-speed limiter.
//
// Reference: seg004.asm loc_21846 (the 16-byte copy) and seg001.asm:6022.

#include <array>
#include <cstdint>
#include <string>

namespace s3d {

class OpponentData {
 public:
  static constexpr int kSpeedCount = 16;
  static constexpr int kOpponentCount = 6;  // OPP1..OPP6

  // `pre_path` points at OPPn.PRE; it is decompressed before parsing.
  bool load(const std::string& pre_path);

  // field_3F9 at a route block: oppnentSped[speed_code + surface]. Indices
  // 0..15 are real table data; a handful of elements push the sum to 16/17,
  // where the original would read adjacent dseg memory. Clamp those to 0.
  uint8_t speed_for(uint8_t speed_code, uint8_t surface) const;

  // The opponent-only engine torque limiter from sped[0] (seg001.asm:6022):
  //   ((0xC8 - sped[0]) >> 1) & 0xFF
  uint8_t engine_torque() const;

  const std::array<uint8_t, kSpeedCount>& speed_table() const { return sped_; }
  const std::string& description() const { return description_; }
  const std::string& name() const { return name_; }

 private:
  std::array<uint8_t, kSpeedCount> sped_{};
  std::string description_;
  std::string name_;
};

// Builds "OPPn.PRE" (n = 1..kOpponentCount) as a file name.
std::string opponent_file(int index);

}  // namespace s3d
