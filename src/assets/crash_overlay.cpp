#include "assets/crash_overlay.h"

#include "assets/compression.h"
#include "assets/res_file.h"

namespace s3d {

namespace {

uint16_t read_u16(const std::vector<uint8_t>& b, size_t off) {
  return static_cast<uint16_t>(b[off] | (b[off + 1] << 8));
}

}  // namespace

bool CrashOverlay::decode(const std::vector<uint8_t>& crak,
                          const std::vector<uint8_t>& cinf) {
  segments_.clear();
  segment_counts_.clear();

  // cinf[0] is the stage count; the following words are the per-stage segment
  // counts (init_crak reads cinf[1 + stage]).
  if (cinf.size() < 2) return false;
  const int stage_count = read_u16(cinf, 0);
  if (stage_count <= 0 || cinf.size() < static_cast<size_t>(2 + 2 * stage_count)) {
    return false;
  }
  for (int i = 0; i < stage_count; ++i) {
    segment_counts_.push_back(read_u16(cinf, 2 + 2 * static_cast<size_t>(i)));
  }

  // crak is a flat array of 8-byte {x1,y1,x2,y2} crack segments.
  if (crak.empty() || crak.size() % 8 != 0) return false;
  segments_.resize(crak.size() / 8);
  for (size_t i = 0; i < segments_.size(); ++i) {
    const size_t off = i * 8;
    segments_[i].x1 = static_cast<int16_t>(read_u16(crak, off + 0));
    segments_[i].y1 = static_cast<int16_t>(read_u16(crak, off + 2));
    segments_[i].x2 = static_cast<int16_t>(read_u16(crak, off + 4));
    segments_[i].y2 = static_cast<int16_t>(read_u16(crak, off + 6));
  }
  return valid();
}

bool CrashOverlay::load_from_game_pre(const std::string& game_pre_path) {
  std::vector<uint8_t> raw;
  if (!decompress_file(game_pre_path, &raw)) return false;
  ResFile rf;
  if (!rf.load(raw.data(), raw.size())) return false;
  return decode(rf.read("crak"), rf.read("cinf"));
}

int CrashOverlay::stage(int framecount, int frames_per_sec) const {
  if (segment_counts_.empty()) return 0;
  const int divisor = frames_per_sec / 7;  // framespersec / 7 (== 2 at 20 Hz)
  int s = divisor > 0 ? framecount / divisor : 0;
  const int max_stage = static_cast<int>(segment_counts_.size()) - 1;
  if (s > max_stage) s = max_stage;
  if (s < 0) s = 0;
  return s;
}

int CrashOverlay::visible_segments(int framecount, int frames_per_sec) const {
  const int s = stage(framecount, frames_per_sec);
  if (s < 0 || s >= static_cast<int>(segment_counts_.size())) return 0;
  return segment_counts_[static_cast<size_t>(s)];
}

}  // namespace s3d
