#pragma once
// 3D shape archives (`GAME1.P3S`, `GAME2.P3S`, and the shapes embedded in
// scene archives).
//
// A `.P3S` file is a compressed resource archive (see `res_file.h`); each
// resource is one raw SHAPE3D:
//
//   struct SHAPE3DHEADER { u8 numverts, numprimitives, numpaints, reserved; };
//   VECTOR  verts[numverts];              // 3 x s16, 6 bytes each
//   u8      visibility[numprimitives*4];  // back-face cull masks
//   u8      front_facing[numprimitives*4];// front-face cull masks
//   primitives[];                         // variable length
//
// Each primitive is `u8 type`, `u8 flags`, `u8 paint[numpaints]`, followed by
// `primidxcounttab[type]` vertex indices. `primtypetab[type]` selects the
// internal primitive kind (1 = line, 5 = point, 0 = filled polygon).
//
// Reference: reference/restunts/src/restunts/c/shape3d.c (shape3d_init_shape)
// and asmorig/dseg.asm (primidxcounttab / primtypetab).

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace s3d {

struct Vec3i {
  int16_t x = 0, y = 0, z = 0;
};

enum class PrimKind : uint8_t { Polygon = 0, Line = 1, Point = 5 };

struct ShapePrimitive {
  uint8_t type = 0;    // raw file primitive type (0..15)
  uint8_t flags = 0;   // raw flags byte
  PrimKind kind = PrimKind::Polygon;
  std::vector<uint8_t> paint;    // one paint index per material (numpaints)
  std::vector<uint8_t> indices;  // vertex indices
};

struct Shape3D {
  std::string name;
  uint8_t num_paints = 0;
  std::vector<Vec3i> vertices;
  std::vector<ShapePrimitive> primitives;
};

// Loads and merges shape archives. GAME1 (track elements) takes precedence over
// GAME2 (scenery) for duplicate names, matching the original loader.
class ShapeLibrary {
 public:
  // Decompresses and parses a `.P3S` archive. Later calls do not overwrite
  // shapes already loaded from an earlier archive.
  bool load_p3s(const std::string& path);

  const Shape3D* find(const std::string& name) const;
  const std::vector<Shape3D>& shapes() const { return shapes_; }

  // Number of track-element shapes in the fixed load order.
  static size_t track_shape_count();
  // Shape name at a fixed load-order index (0..track_shape_count()-1), or "".
  static const char* track_shape_name(size_t index);

  // Parses a single raw shape out of `data` (length `size`) at `offset`.
  // Returns false and leaves `out` untouched on malformed input.
  static bool parse_shape(const uint8_t* data, size_t size, size_t offset, Shape3D* out);

 private:
  std::vector<Shape3D> shapes_;
  std::unordered_map<std::string, size_t> by_name_;
};

}  // namespace s3d
