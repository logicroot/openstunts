#include "world/track_route.h"

#include <array>

#include "world/track_objects.h"
#include "world/track_route_data.h"

namespace s3d::world {

const char* route_error_name(RouteError e) {
  switch (e) {
    case RouteError::none: return "none";
    case RouteError::int_err: return "int_err";
    case RouteError::elem_mism: return "elem_mism";
    case RouteError::wrong_way: return "wrong_way";
    case RouteError::no_runway: return "no_runway";
    case RouteError::long_jump: return "long_jump";
    case RouteError::many_path: return "many_path";
    case RouteError::many_elem: return "many_elem";
    case RouteError::no_path: return "no_path";
    case RouteError::no_sf: return "no_sf";
    case RouteError::many_sf: return "many_sf";
    case RouteError::jump: return "jump";
  }
  return "?";
}

namespace {

constexpr int kGrid = Track::kSize;        // 30
constexpr int kGridCount = kGrid * kGrid;  // 900
constexpr int kMaxNodes = 0x385;           // the original's array size
constexpr int kMaxDepth = 0x40;            // the original's tcomp stack size

// subst_hillroad_track (seg004.asm 6109): on hill terrain (7..10) certain
// road/deck elements are replaced by the matching hill-road element
// (0xB6..0xC5). Anything else maps to 0, which the walker reads as empty.
uint8_t subst_hillroad(int terrain, uint8_t item) {
  if (terrain < 7 || terrain > 10) return 0;
  static constexpr uint8_t kPieces[4][6] = {
      {4, 14, 24, 39, 59, 98},
      {5, 15, 25, 36, 56, 95},
      {4, 14, 24, 38, 58, 97},
      {5, 15, 25, 37, 57, 96}};
  static constexpr uint8_t kOut[4][4] = {
      {0xB6, 0xBA, 0xBE, 0xC2},
      {0xB7, 0xBB, 0xBF, 0xC3},
      {0xB8, 0xBC, 0xC0, 0xC4},
      {0xB9, 0xBD, 0xC1, 0xC5}};
  const int r = terrain - 7;
  for (int i = 0; i < 6; ++i) {
    if (kPieces[r][i] == item) return kOut[r][i < 3 ? i : 3];
  }
  return 0;
}

// Direction -> entry point for a regular element (seg004.asm loc_20C2A).
// 0 = north, 0x100 = east, 0x200 = south, 0x300 = west.
int normal_entry_point(int dir) {
  switch (dir) {
    case 0x000: return 2;
    case 0x100: return 4;
    case 0x200: return 1;
    case 0x300: return 3;
  }
  return 0;
}

// The diagonal filler bytes encode which diagonal they continue; a direction
// they do not serve yields 0 (an int_err for the walk).
int entry_point_0xfd(int dir) {
  switch (dir) {
    case 0x000: return 0x0C;
    case 0x300: return 9;
    default: return 0;
  }
}
int entry_point_0xfe(int dir) {
  switch (dir) {
    case 0x000: return 0x0B;
    case 0x100: return 6;
    case 0x300: return 7;
    default: return 0;
  }
}
int entry_point_0xff(int dir) {
  switch (dir) {
    case 0x000: return 0x0A;
    case 0x200: return 5;
    case 0x300: return 8;
    default: return 0;
  }
}

// One saved fork alternative (the original's 14-byte tcomp entry). Only the
// fields the route walk itself needs are kept; the rest belong to the skipped
// camera/course phase.
struct StackEntry {
  int col;
  int row;
  int elem;
  int sub;
  int conn;
  int runway;
  int prev_conn_code;
  int prev_node;
};

int block_count(uint8_t elem) {
  if (elem >= kRouteElementCount) return 0;
  return kRouteElementBlockCount[elem];
}
const RouteBlockDef& block(uint8_t elem, int sub) {
  return kRouteBlocks[kRouteElementBlockOffset[elem] + sub];
}

// Absolute dseg byte offset -> the signed 16-bit camera-data words it points at
// (seg001.asm sub_18D60's es:[bx+si] reads).
const int16_t* camera_data(uint16_t offset) {
  return &kCameraData[(offset - kCameraDataBase) / 2];
}

// Cell height bump sub_18D60 adds on terrain type 6 (hillHeightConsts[1]).
constexpr int kElevatedHeight = 450;

}  // namespace

bool TrackRoute::build(const Track& track, int start_col, int start_row,
                       int start_angle) {
  const auto fail = [this](RouteError e) {
    error_ = e;
    return false;
  };

  error_ = RouteError::none;
  node_count_ = 0;
  start_col_ = start_col;
  start_row_ = start_row;

  td01_.assign(kMaxNodes, -1);
  td02_.assign(kMaxNodes, -1);
  nodes_.assign(kMaxNodes, RouteNode{});
  std::array<uint8_t, kGridCount> visited{};   // var_738
  std::array<uint8_t, kMaxNodes> node_sub{};   // var_AD4
  std::array<uint8_t, kMaxNodes> node_conn{};  // var_398
  std::array<StackEntry, kMaxDepth> stack{};

  int col = start_col;
  int row = start_row;
  int direction = start_angle;
  int prev_conn_code = 0;
  int prev_node = -1;
  int jump = 0;
  int runway = 0;
  int started = 0;
  int depth = 0;
  int counter = 0;
  int prev_col = 0;
  int prev_row = 0;

  for (;;) {
    int var2 = 0;
    bool pop_now = false;
    int elem = 0;
    int sub_block = 0;
    int conn_status = 0;

    if (col < 0 || col >= kGrid || row < 0 || row >= kGrid) {
      pop_now = true;  // loc_20A0C
    } else {
      // loc_20AC2: read the cell's element and terrain (terrain rows are the
      // mirror of the item rows), then substitute hill-road pieces.
      elem = track.item(col, kGrid - 1 - row);
      const int terrain = track.terrain_at(col, kGrid - 1 - row);
      if (elem != 0 && terrain >= 7 && terrain < 11) {
        elem = subst_hillroad(terrain, static_cast<uint8_t>(elem));
      }

      int entry_point = 0;
      if (elem >= 0xFD) {
        // loc_20B76/BA2/BD4: diagonal fillers shift onto the piece's base cell
        // and encode the entry from the arrival direction.
        if (elem == 0xFD) {
          --col;
          --row;
          entry_point = entry_point_0xfd(direction);
        } else if (elem == 0xFE) {
          --row;
          entry_point = entry_point_0xfe(direction);
        } else {
          --col;
          entry_point = entry_point_0xff(direction);
        }
        // loc_20B50 reloads the element at the adjusted cell.
        elem = (col >= 0 && col < kGrid && row >= 0 && row < kGrid)
                   ? track.item(col, kGrid - 1 - row)
                   : 0;
      } else {
        entry_point = normal_entry_point(direction);
      }

      if (jump == 0 && entry_point == 0) return fail(RouteError::int_err);

      // loc_20C2E: pick the first matching block; further matches are pushed
      // as fork alternatives.
      const int nb = block_count(static_cast<uint8_t>(elem));
      for (int si = 0; si < nb; ++si) {
        const RouteBlockDef& b = block(static_cast<uint8_t>(elem), si);
        int conn_flag = -1;
        if (b.entry_point == entry_point) {
          if (b.entry_type != prev_conn_code) return fail(RouteError::elem_mism);
          conn_flag = 0;
        } else if (b.exit_point == entry_point) {
          if (b.exit_type != prev_conn_code) return fail(RouteError::elem_mism);
          conn_flag = 1;
        }
        if (conn_flag < 0) continue;

        // loc_20CAC: closing back onto an already-visited cell.
        if (visited[(kGrid - 1 - row) * kGrid + col]) {
          for (int di = 0; di < counter; ++di) {
            if (nodes_[di].col != col || nodes_[di].row != row) continue;
            if (node_sub[di] != si) continue;
            if (node_conn[di] != conn_flag) return fail(RouteError::wrong_way);
            conn_flag = -1;  // linked; the block is consumed
            if (td01_[prev_node] == -1) td01_[prev_node] = di;
            else td02_[prev_node] = di;
            if (di == 0) started = 1;
          }
        }
        if (conn_flag < 0) continue;

        // loc_20D48 / loc_20D52.
        if (var2 == 0) {
          sub_block = si;
          conn_status = conn_flag;
        } else {
          if (depth == kMaxDepth) return fail(RouteError::many_path);
          stack[depth++] = StackEntry{col,      row,           elem,  si,
                                      conn_flag, runway,        prev_conn_code,
                                      prev_node};
        }
        ++var2;
      }

      if (var2 == 0) {
        // loc_20E8B: nothing matched. Either jump the gap or give up.
        if (prev_conn_code == 1) {
          if (jump >= 2) {
            pop_now = true;
          } else {
            if (runway < 2) return fail(RouteError::no_runway);
            ++runway;
            ++jump;
            // loc_20EAC: launch from the previous node's cell across `jump`
            // empty cells.
            switch (direction) {
              case 0x000: col = prev_col; row = prev_row - jump - 1; break;
              case 0x200: col = prev_col; row = prev_row + jump + 1; break;
              case 0x100: row = prev_row; col = prev_col + jump + 1; break;
              case 0x300: row = prev_row; col = prev_col - jump - 1; break;
            }
            continue;  // loc_209E9
          }
        } else {
          pop_now = true;
        }
      }
    }

    if (pop_now) {
      if (depth == 0) {
        if (!started) return fail(RouteError::no_path);
        break;  // loc_20F20: done
      }
      const StackEntry& s = stack[--depth];
      col = s.col;
      row = s.row;
      elem = s.elem;
      sub_block = s.sub;
      conn_status = s.conn;
      prev_conn_code = s.prev_conn_code;
      prev_node = s.prev_node;
      runway = s.runway;
      var2 = 1;
    }

    if (jump > 1) return fail(RouteError::long_jump);
    jump = 0;

    // loc_20F6A: record the node and wire the edge from its predecessor.
    visited[(kGrid - 1 - row) * kGrid + col] = 1;
    node_sub[counter] = static_cast<uint8_t>(sub_block);
    node_conn[counter] = static_cast<uint8_t>(conn_status);
    if (prev_node != -1) {
      if (td01_[prev_node] == -1) td01_[prev_node] = counter;
      else td02_[prev_node] = counter;
    }
    prev_node = counter;
    nodes_[counter].col = static_cast<uint8_t>(col);
    nodes_[counter].row = static_cast<uint8_t>(row);
    nodes_[counter].elem = static_cast<uint8_t>(elem);
    nodes_[counter].sub_block = static_cast<uint8_t>(sub_block);
    nodes_[counter].conn_status = static_cast<uint8_t>(conn_status);

    // loc_21047: si_opp3 == 0 marks a runway piece; anything else resets the
    // runway counter (the original's course-checkpoint branch does the same).
    if (block(static_cast<uint8_t>(elem), sub_block).opp3 == 0) ++runway;
    else runway = 0;

    ++counter;
    if (counter >= kMaxNodes) return fail(RouteError::many_elem);

    // loc_2137C: leave the cell along the block's opposite connection.
    const RouteBlockDef& b = block(static_cast<uint8_t>(elem), sub_block);
    int exit_point;
    if (conn_status != 0) {
      exit_point = b.entry_point;
      prev_conn_code = b.entry_type;
    } else {
      exit_point = b.exit_point;
      prev_conn_code = b.exit_type;
    }
    prev_col = col;
    prev_row = row;

    // off_2147C: exit point -> grid step + new direction.
    const unsigned idx = static_cast<unsigned>(exit_point) - 1;
    if (idx <= 0xBu) {
      switch (idx) {
        case 0: --row; direction = 0x000; break;
        case 1: ++row; direction = 0x200; break;
        case 2: ++col; direction = 0x100; break;
        case 3: --col; direction = 0x300; break;
        case 4: --row; ++col; direction = 0x000; break;
        case 5: ++row; --col; direction = 0x300; break;
        case 6: ++col; ++row; direction = 0x100; break;
        case 7: col += 2; direction = 0x100; break;
        case 8: col += 2; ++row; direction = 0x100; break;
        case 9: ++col; ++row; direction = 0x200; break;
        case 10: row += 2; direction = 0x200; break;
        case 11: ++col; row += 2; direction = 0x200; break;
      }
    }
  }

  node_count_ = counter;
  built_ = true;
  return true;
}

bool TrackRoute::checkpoint(int node, int index, const Track& track,
                            RouteCheckpoint* out) const {
  if (node < 0 || node >= node_count_ || out == nullptr) return false;
  const RouteNode& n = nodes_[static_cast<size_t>(node)];
  const RouteBlockDef& b = block(n.elem, n.sub_block);

  // The sped-table index inputs (field_3F9 = oppnentSped[speed_code + surface]).
  out->speed_code = b.opp_sped_code;
  out->surface = world::track_object(n.elem).surface;

  const int arrow_type = b.arrow_type;
  if (arrow_type <= 0 || index < 0 || index >= arrow_type) return false;

  // trackdata18 = (connStatus << 4) | subTOI; the entry side counts up from the
  // block's first point, the exit side counts back from its last.
  const int step = (n.conn_status == 0) ? index * 2 : (arrow_type - index) * 2 - 2;

  // A block entered through its exit point normally reads its camera-data pair
  // back-to-front; a nonzero si_opp1:si_opp2 pointer overrides the table
  // entirely (var_24).
  const int16_t* base;
  bool swap = false;
  int opp1 = 0;
  if (n.conn_status != 0 && b.opp1_word != 0) {
    base = camera_data(b.opp1_word);
    opp1 = 1;
  } else {
    base = camera_data(b.camera_offset);
    swap = (n.conn_status != 0);
  }

  const int16_t* p = base + step * 3;   // 6*step bytes == 3*step words
  int ax = p[0], ay = p[1], az = p[2];
  int bx = p[3], by = p[4], bz = p[5];
  if (swap) {
    int t;
    t = ax; ax = bx; bx = t;
    t = ay; ay = by; by = t;
    t = az; az = bz; bz = t;
  }

  // si_arrowOrient rotates the local points in the cell's XZ plane.
  const auto rotate = [orient = b.arrow_orient](int& x, int& z) {
    int nx, nz;
    switch (orient) {
      case 0x100: nx = z; nz = -x; break;
      case 0x200: nx = -x; nz = -z; break;
      case 0x300: nx = -z; nz = x; break;
      default: return;
    }
    x = nx;
    z = nz;
  };
  rotate(ax, az);
  rotate(bx, bz);

  // Elevated terrain lifts both points by hillHeightConsts[1].
  if (ay != -1 && track.terrain_at(n.col, kGrid - 1 - n.row) == 6) {
    ay += kElevatedHeight;
    by += kElevatedHeight;
  }

  // Cell position: the center of the node's cell, or the far corner when the
  // element's ss_multiTileFlag covers more than one square.
  const int multi = kRouteElementMultiTile[n.elem];
  const int file_row = kGrid - 1 - n.row;          // 0 = top row
  const int row_off = (multi & 1) ? file_row * 1024 : file_row * 1024 + 512;
  const int col_off = (multi & 2) ? (n.col + 1) * 1024 : n.col * 1024 + 512;
  az += row_off;
  bz += row_off;
  ax += col_off;
  bx += col_off;

  out->x = (ax + bx) >> 1;
  out->y = (ay == -1) ? -1 : (ay + by) >> 1;
  out->z = (az + bz) >> 1;
  out->ax = ax;
  out->ay = ay;
  out->az = az;
  out->bx = bx;
  out->by = by;
  out->bz = bz;
  out->opp1 = opp1;
  return index == arrow_type - 1;
}

std::vector<uint16_t> TrackRoute::opponent_path(const uint8_t* sped,
                                                int sped_len) const {
  // The original accumulates a 32-bit cost and starts `best` at 0x0F423F.
  constexpr int32_t kNoCost = 0x0F423F;
  std::vector<uint16_t> list;   // the current DFS path (the original's L[])
  std::vector<uint16_t> out;    // trackdata3
  struct Frame {
    int node;
    int len;          // list length to restore (var_B28)
    int32_t cost;
  };
  std::vector<Frame> stack;

  const auto add = [&](int node) -> int32_t {
    const int e = nodes_[static_cast<size_t>(node)].elem;
    // load_opponent_data reads the 16-byte 'sped' chunk by element, so for
    // elements >= 16 the original runs past the chunk into the next resource
    // in the decompressed file. Clamp those to 0 (weight 1).
    const int v = (e >= 0 && e < sped_len) ? sped[e] : 0;
    return static_cast<int32_t>(v) + 1;
  };

  int32_t best = kNoCost;
  int32_t cost = 0;
  int si = 0;

  for (;;) {
    int last = 0;   // var_2: primary successor is 0 (a completed lap)
    int stop = 0;   // var_410: do not descend, compare/backtrack
    const int next = successor(si);  // td01; 0 = wrap to node 0, -1 = none
    if (next == 0) {
      last = 1;
      stop = 1;
    } else if (next == -1) {
      stop = 1;
    } else if (!list.empty()) {
      for (size_t di = 0; di < list.size(); ++di) {
        if (list[di] == static_cast<uint16_t>(si)) stop = 1;
      }
    }

    // loc_218DA: record the node and add its element's weight.
    list.push_back(static_cast<uint16_t>(si));
    cost += add(si);

    if (stop == 0) {
      // loc_219FE: stash the secondary successor, descend the primary one.
      const int alt = alternate(si);
      if (alt != -1) stack.push_back(Frame{alt, static_cast<int>(list.size()), cost});
      si = next;
      continue;
    }

    // loc_2191A: if the lap closed, keep it when strictly cheaper.
    if (last != 0 && cost < best) {
      best = cost;
      list.push_back(0);  // L[var_B28] = 0; the list terminator
      out.assign(list.begin(), list.end());
      out.push_back(0);
      out.push_back(1);
    }

    // loc_219A5: backtrack to the most recent unexplored branch.
    if (stack.empty()) break;
    const Frame f = stack.back();
    stack.pop_back();
    si = f.node;
    list.resize(static_cast<size_t>(f.len));
    cost = f.cost;
  }

  return out;
}

}  // namespace s3d::world
