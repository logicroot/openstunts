#pragma once
// Track route graph built by the original track-setup pass (seg004.asm
// `track_setup`). The walker follows every element's TRKOBJINFO entry/exit
// connections across the 30x30 item grid and produces the graph the wrong-way /
// off-track penalty system (seg001.asm `detect_penalty`) consumes:
//
//   * td01_track_file_cpy / td02_penalty_related: each node's primary and
//     secondary successor (-1 when absent);
//   * td17_trk_elem_ordered: the element each node sits on (after the
//     subst_hillroad_track substitution);
//   * td21_col_from_path / td22_row_from_path: the node's cell, in the
//     original bottom-up rows (row = 29 - file row);
//   * track_pieces_counter: the node count.
//
// The original builds camera, arrow, opponent-start and course-checkpoint
// tables in the same pass; those are not ported (see track_route.cpp).

#include <cstdint>
#include <vector>

#include "assets/trk.h"

namespace s3d::world {

// Track setup error codes (seg004.asm `trackErrorCode`).
enum class RouteError : uint8_t {
  none = 0,
  int_err,    // internal inconsistency (entry point 0 with no jump)
  elem_mism,  // connection type does not match the previous block
  wrong_way,  // a second node at one cell disagrees on the connection
  no_runway,  // a gap is reached before enough runway was built up
  long_jump,  // a jump spans more than one cell
  many_path,  // the fork stack overflowed (0x40)
  many_elem,  // more route nodes than the 0x385 arrays hold
  no_path,    // the walk never closed back to node 0
  no_sf,      // no start/finish element
  many_sf,    // more than one start/finish element
  jump,
};

const char* route_error_name(RouteError e);

// One route node: the cell it occupies and the TRKOBJINFO block that got it
// there. `row` is the original bottom-up row (0 = the file's last row).
struct RouteNode {
  uint8_t col = 0;
  uint8_t row = 0;
  uint8_t elem = 0;        // element after subst_hillroad_track
  uint8_t sub_block = 0;   // TRKOBJINFO block index
  uint8_t conn_status = 0; // 0 = entered via entryPoint, 1 = via exitPoint
};

class TrackRoute {
 public:
  // Walks the track starting on the start/finish cell. `start_col` and
  // `start_row` use the original convention (column, bottom-up row);
  // `start_angle` is the start/finish direction (0 / 0x100 / 0x200 / 0x300).
  // Returns true on success; on failure `error()` carries the reason and the
  // graph is not usable.
  bool build(const Track& track, int start_col, int start_row, int start_angle);

  bool valid() const { return built_ && error_ == RouteError::none; }
  RouteError error() const { return error_; }

  bool built() const { return built_; }
  int node_count() const { return node_count_; }

  // Successor node indices, -1 when absent (td01 / td02).
  int successor(int node) const {
    if (node < 0 || node >= node_count_) return -1;
    return td01_[static_cast<size_t>(node)];
  }
  int alternate(int node) const {
    if (node < 0 || node >= node_count_) return -1;
    return td02_[static_cast<size_t>(node)];
  }
  const RouteNode& node(int i) const { return nodes_[static_cast<size_t>(i)]; }

  int start_col() const { return start_col_; }
  int start_row() const { return start_row_; }

 private:
  static constexpr int kMaxNodes = 0x385;  // the original's array size

  std::vector<RouteNode> nodes_;
  std::vector<int> td01_;
  std::vector<int> td02_;
  bool built_ = false;
  int node_count_ = 0;
  int start_col_ = 0;
  int start_row_ = 0;
  RouteError error_ = RouteError::none;
};

}  // namespace s3d::world
