#pragma once
// Classification of track item / terrain bytes into game concepts.
//
// Item byte ranges are taken from reference/restunts/docs/format.html. The
// authoritative element->shape tables (used to build 3D geometry) are being
// extracted from the restunts disassembly; this header only provides coarse
// categories used for placeholder rendering and physics surface selection.

#include <cstdint>

namespace s3d::items {

enum class Surface {
  None,       // not a drivable surface (scenery / solid)
  Road,       // paved road
  Dirt,       // unpaved road
  Ice,        // icy road
  Raised,     // raised road section
  Bridge,     // bridge deck
  Special,    // loops, corks, pipes, tunnels, ramps...
  Scenery,    // obstacles, buildings, trees...
};

inline bool in_range(uint8_t v, uint8_t lo, uint8_t hi) { return v >= lo && v <= hi; }

// Coarse surface of an item byte. Ranges follow the documented item list.
inline Surface surface_of(uint8_t item) {
  if (item == 0x00) return Surface::None;
  if (in_range(item, 0x04, 0x0d)) return Surface::Road;
  if (in_range(item, 0x0e, 0x17)) return Surface::Dirt;
  if (in_range(item, 0x18, 0x21)) return Surface::Ice;
  if (in_range(item, 0x22, 0x27)) return Surface::Raised;
  if (in_range(item, 0x38, 0x3b)) return Surface::Bridge;
  if (item == 0x4a) return Surface::Road;  // cross road
  if (in_range(item, 0x5f, 0x6f)) return Surface::Special;  // solid ramps/road
  if (in_range(item, 0x28, 0x37)) return Surface::Special;  // specials/scenery
  if (item >= 0x3c) return Surface::Special;
  if (item >= 0x01) return Surface::Scenery;
  return Surface::None;
}

inline bool is_drivable(uint8_t item) {
  const Surface s = surface_of(item);
  return s == Surface::Road || s == Surface::Dirt || s == Surface::Ice ||
         s == Surface::Raised || s == Surface::Bridge;
}

}  // namespace s3d::items
