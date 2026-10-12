#pragma once
// Crashed-windscreen overlay data (restunts seg003.asm init_crak).
//
// When the player car is in the collision crash state (car_crashBmpFlag == 1)
// the original draws a cracked-windscreen pattern over the view: a set of line
// segments that grow over the first few frames of the crash and then stay. The
// pattern and its per-frame progress live in two GAME.PRE chunks:
//
//   "cinf"  u16 stage_count, then u16 segment_counts[stage_count]
//   "crak"  stage-ordered array of 8-byte segments
//           { i16 x1, i16 y1, i16 x2, i16 y2 } in 320x200 view coordinates.
//
// init_crak shows the first `segment_counts[stage]` segments, where
//
//   stage = min((game_frame - game_pEndFrame) / (framespersec / 7),
//               stage_count - 1)
//
// (framespersec is the 20 Hz base rate, so framespersec / 7 == 2). The y
// coordinates are scaled by the view height / 200; x is used as authored.
//
// Reference: reference/restunts/src/restunts/asmorig/seg003.asm (init_crak).

#include <cstdint>
#include <string>
#include <vector>

namespace s3d {

struct CrashCrack {
  int16_t x1 = 0;
  int16_t y1 = 0;
  int16_t x2 = 0;
  int16_t y2 = 0;
};

class CrashOverlay {
 public:
  // Loads "crak"/"cinf" from GAME.PRE (decompressed with the .PRE codec).
  bool load_from_game_pre(const std::string& game_pre_path);

  // Decodes already-read chunks. Exposed so tests can feed synthetic data.
  bool decode(const std::vector<uint8_t>& crak, const std::vector<uint8_t>& cinf);

  bool valid() const { return !segments_.empty() && !segment_counts_.empty(); }
  const std::vector<CrashCrack>& segments() const { return segments_; }
  int stage_count() const { return static_cast<int>(segment_counts_.size()); }

  // init_crak's `var_1A` (the active stage) from the crash frame count and the
  // base frames-per-second rate (restunts `framespersec`).
  int stage(int framecount, int frames_per_sec) const;

  // init_crak's `var_18`: how many leading segments are drawn for `framecount`.
  int visible_segments(int framecount, int frames_per_sec) const;

 private:
  std::vector<CrashCrack> segments_;
  std::vector<uint16_t> segment_counts_;
};

// do_sinking's fill height (seg003.asm do_sinking, frame.c loc_1162). When the
// player car is in the water crash state (car_crashBmpFlag == 2) the original
// floods the view from the bottom with skybox_wat_color: a rect [0, 320] wide
// whose height grows from 0 to `height` over `framespersec << 2` game frames.
//
//   span = frames_per_sec << 2
//   fill = min(framecount, span) * height / span
//
// Returns 0 when frames_per_sec is non-positive (span == 0); a negative
// framecount clamps to 0 (game_frame - game_pEndFrame is always >= 0).
int sinking_fill_height(int framecount, int height, int frames_per_sec);

}  // namespace s3d
