#include "world/track_mesh.h"

#include <glm/glm.hpp>

#include <array>
#include <unordered_map>
#include <vector>

#include "core/log.h"
#include "world/mesh_builder.h"
#include "world/track_items.h"
#include "world/world_units.h"

namespace s3d {

namespace {

using namespace s3d::world;

struct CellCorners {
  float tl, tr, bl, br;  // multiples of kHillHeight
};

// Provisional terrain corner-height model. To be confirmed against the
// original terrain tables in restunts (see docs/format.html "Terrain").
CellCorners terrain_corners(uint8_t terrain) {
  switch (terrain) {
    case 0x00: return {0, 0, 0, 0};
    case 0x01: return {-10, -10, -10, -10};  // water (flattened below sea level)
    case 0x02: case 0x03: case 0x04: case 0x05:
      return {0, 0, 0, 0};  // coast: approximated as flat land
    case 0x06: return {1, 1, 1, 1};  // elevated flat
    case 0x07: return {1, 1, 0, 0};  // slope low bottom
    case 0x08: return {1, 0, 1, 0};  // slope low right
    case 0x09: return {0, 0, 1, 1};  // slope low top
    case 0x0a: return {0, 1, 0, 1};  // slope low left
    case 0x0b: return {1, 0, 0, 0};  // diagonal, top-left raised
    case 0x0c: return {0, 0, 1, 0};  // diagonal, bottom-left raised
    case 0x0d: return {0, 0, 0, 1};  // diagonal, bottom-right raised
    case 0x0e: return {0, 1, 0, 0};  // diagonal, top-right raised
    case 0x0f: return {1, 1, 1, 0};  // three raised, bottom-right low
    case 0x10: return {1, 0, 1, 1};  // three raised, top-right low
    case 0x11: return {0, 1, 1, 1};  // three raised, top-left low
    case 0x12: return {1, 1, 0, 1};  // three raised, bottom-left low
    default: return {0, 0, 0, 0};
  }
}

glm::vec4 terrain_color(uint8_t terrain) {
  if (terrain == 0x01) return {0.15f, 0.35f, 0.75f, 1.0f};                  // water
  if (terrain >= 0x02 && terrain <= 0x05) return {0.78f, 0.72f, 0.45f, 1.0f}; // coast/sand
  if (terrain == 0x06) return {0.28f, 0.48f, 0.22f, 1.0f};                  // hill top
  if (terrain >= 0x07 && terrain <= 0x12) return {0.33f, 0.50f, 0.24f, 1.0f}; // slope
  return {0.30f, 0.52f, 0.24f, 1.0f};                                       // flat land
}

glm::vec4 surface_color(items::Surface s) {
  switch (s) {
    case items::Surface::Road: return {0.22f, 0.22f, 0.24f, 1.0f};
    case items::Surface::Dirt: return {0.45f, 0.33f, 0.20f, 1.0f};
    case items::Surface::Ice: return {0.70f, 0.82f, 0.90f, 1.0f};
    case items::Surface::Raised: return {0.30f, 0.30f, 0.34f, 1.0f};
    case items::Surface::Bridge: return {0.55f, 0.45f, 0.30f, 1.0f};
    default: return {0.60f, 0.30f, 0.60f, 1.0f};
  }
}

struct Key {
  int x, y;
  bool operator==(const Key& o) const { return x == o.x && y == o.y; }
};
struct KeyHash {
  size_t operator()(const Key& k) const {
    return (static_cast<size_t>(k.y) << 16) ^ static_cast<size_t>(k.x);
  }
};

}  // namespace

Mesh build_track_mesh(const Track& track, TrackMeshStats* stats) {
  TrackMeshStats local;
  MeshBuilder mb;

  // Vertex height = average of the corner heights of adjacent cells, avoiding
  // cracks between neighbouring slope cells.
  std::unordered_map<Key, float, KeyHash> vertex_height;
  auto contribute = [&](int vx, int vy, float h) {
    vertex_height[Key{vx, vy}] += h;
  };
  std::unordered_map<Key, int, KeyHash> vertex_count;

  auto add_cell_corners = [&](int x, int y, const CellCorners& c) {
    // vertex (x,y)   -> tl, (x+1,y) -> tr, (x,y+1) -> bl, (x+1,y+1) -> br
    contribute(x, y, c.tl);
    contribute(x + 1, y, c.tr);
    contribute(x, y + 1, c.bl);
    contribute(x + 1, y + 1, c.br);
    ++vertex_count[Key{x, y}];
    ++vertex_count[Key{x + 1, y}];
    ++vertex_count[Key{x, y + 1}];
    ++vertex_count[Key{x + 1, y + 1}];
  };

  std::array<CellCorners, Track::kCount> corners;
  for (int y = 0; y < kTrackSize; ++y) {
    for (int x = 0; x < kTrackSize; ++x) {
      const uint8_t t = track.terrain_at(x, y);
      corners[static_cast<size_t>(y) * kTrackSize + x] = terrain_corners(t);
      if (t == 0x01) ++local.water_cells;
    }
  }
  for (int y = 0; y < kTrackSize; ++y) {
    for (int x = 0; x < kTrackSize; ++x) {
      add_cell_corners(x, y, corners[static_cast<size_t>(y) * kTrackSize + x]);
    }
  }

  auto height_at = [&](int vx, int vy) -> float {
    const Key k{vx, vy};
    const auto it = vertex_height.find(k);
    const auto cit = vertex_count.find(k);
    if (it == vertex_height.end() || cit == vertex_count.end() || cit->second == 0) {
      return 0.0f;
    }
    return (it->second / static_cast<float>(cit->second)) * kHillHeight;
  };

  // Terrain mesh.
  for (int y = 0; y < kTrackSize; ++y) {
    for (int x = 0; x < kTrackSize; ++x) {
      const uint8_t t = track.terrain_at(x, y);
      const glm::vec4 color = terrain_color(t);
      const float x0 = static_cast<float>(x) * kSquareSize;
      const float x1 = static_cast<float>(x + 1) * kSquareSize;
      const float z0 = static_cast<float>(y) * kSquareSize;
      const float z1 = static_cast<float>(y + 1) * kSquareSize;
      const glm::vec3 a(x0, height_at(x, y), z0);
      const glm::vec3 b(x1, height_at(x + 1, y), z0);
      const glm::vec3 c(x1, height_at(x + 1, y + 1), z1);
      const glm::vec3 d(x0, height_at(x, y + 1), z1);
      mb.add_quad(a, b, c, d, color, glm::vec3(0.0f, 1.0f, 0.0f));
      ++local.terrain_quads;
    }
  }

  // Drivable surface slabs (placeholder until real element shapes are wired).
  for (int y = 0; y < kTrackSize; ++y) {
    for (int x = 0; x < kTrackSize; ++x) {
      const uint8_t item = track.item(x, y);
      if (item == 0) continue;
      const items::Surface s = items::surface_of(item);
      if (s == items::Surface::None || s == items::Surface::Scenery) continue;

      const float h_center = 0.25f * (height_at(x, y) + height_at(x + 1, y) +
                                      height_at(x, y + 1) + height_at(x + 1, y + 1));
      const float lift = (s == items::Surface::Raised || s == items::Surface::Bridge)
                             ? kHillHeight * 0.5f
                             : 3.0f;
      const float hy = h_center + lift;
      const float x0 = static_cast<float>(x) * kSquareSize + kSquareSize * 0.06f;
      const float x1 = static_cast<float>(x + 1) * kSquareSize - kSquareSize * 0.06f;
      const float z0 = static_cast<float>(y) * kSquareSize + kSquareSize * 0.06f;
      const float z1 = static_cast<float>(y + 1) * kSquareSize - kSquareSize * 0.06f;

      if (s == items::Surface::Special) {
        // Marker pillar so special elements are visible in M1.
        mb.add_box(glm::vec3(x0, h_center, z0), glm::vec3(x1, hy + kHillHeight * 0.5f, z1),
                   surface_color(s));
        ++local.special_cells;
        continue;
      }
      mb.add_quad(glm::vec3(x0, hy, z1), glm::vec3(x1, hy, z1),
                  glm::vec3(x1, hy, z0), glm::vec3(x0, hy, z0), surface_color(s),
                  glm::vec3(0.0f, 1.0f, 0.0f));
      ++local.road_quads;
    }
  }

  if (stats != nullptr) *stats = local;
  S3D_INFO("track mesh: %d terrain quads, %d surface quads, %d special cells, %d water cells",
           local.terrain_quads, local.road_quads, local.special_cells, local.water_cells);
  return mb.to_mesh();
}

}  // namespace s3d
