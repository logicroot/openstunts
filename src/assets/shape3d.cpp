#include "assets/shape3d.h"

#include <cstring>

#include "assets/compression.h"
#include "assets/res_file.h"
#include "core/log.h"

namespace s3d {
namespace {

constexpr size_t kHeaderSize = 4;
constexpr size_t kVertexRecordSize = 6;
constexpr size_t kCullRecordSize = 4;
constexpr uint8_t kPrimIdxCount[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 2, 6, 3, 0, 0};
constexpr uint8_t kPrimType[16] = {0, 5, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 4, 0, 0};

inline int16_t read_i16(const uint8_t* p) {
  return static_cast<int16_t>(static_cast<uint16_t>(p[0]) |
                              (static_cast<uint16_t>(p[1]) << 8));
}

// Fixed load order of the 116 track/scenery shapes, from the original
// `game_shape_names` table (restunts full_strings.c). Index into this array is
// the index used by `game3dshapes[]` / `trkObjectList`.
const char* const kTrackShapeNames[] = {
    "barn", "zbrn", "brid", "zbri", "btur", "zbtu", "chi1", "zch1", "chi2", "zch2",
    "elrd", "zelr", "fini", "zfin", "gass", "zgas", "lban", "zlba", "loop", "zloo",
    "offi", "zoff", "pipe", "zpip", "ramp", "zram", "rban", "zrba", "rdup", "zrdu",
    "road", "zroa", "stur", "zstu", "tenn", "zten", "tunn", "ztun", "turn", "ztur",
    "goui", "gouo", "goup", "high", "lakc", "lake", "cld1", "cld2", "cld3", "sigl",
    "sigr", "tree", "inte", "zint", "offl", "zoff", "offr", "zofr", "palm", "zpal",
    "bank", "zban", "sofl", "zsol", "sofr", "zsor", "sram", "zsra", "selr", "zser",
    "elsp", "zesp", "cact", "cact", "spip", "zspi", "sest", "zses", "wroa", "zwro",
    "barr", "zbar", "lco0", "zlco", "rco0", "zrco", "gwro", "zgwr", "lco1", "rco1",
    "loo1", "hig1", "hig2", "hig3", "wind", "zwin", "boat", "zboa", "rest", "zres",
    "hpip", "zhpi", "vcor", "zvco", "tun2", "pip2", "fenc", "zfen", "cfen", "zcfe",
    "flag", "truk", "exp0", "exp1", "exp2", "exp3",
};

}  // namespace

bool ShapeLibrary::parse_shape(const uint8_t* data, size_t size, size_t offset,
                               Shape3D* out) {
  if (out == nullptr) return false;
  if (offset + kHeaderSize > size) return false;

  const uint8_t numverts = data[offset + 0];
  const uint8_t numprims = data[offset + 1];
  const uint8_t numpaints = data[offset + 2];

  const size_t verts_off = offset + kHeaderSize;
  const size_t cull1_off = verts_off + static_cast<size_t>(numverts) * kVertexRecordSize;
  const size_t cull2_off = cull1_off + static_cast<size_t>(numprims) * kCullRecordSize;
  const size_t prims_off = cull2_off + static_cast<size_t>(numprims) * kCullRecordSize;
  if (prims_off > size) return false;

  Shape3D shape;
  shape.num_paints = numpaints;
  shape.vertices.resize(numverts);
  for (uint8_t i = 0; i < numverts; ++i) {
    const uint8_t* v = data + verts_off + static_cast<size_t>(i) * kVertexRecordSize;
    shape.vertices[i] = {read_i16(v), read_i16(v + 2), read_i16(v + 4)};
  }

  size_t p = prims_off;
  shape.primitives.reserve(numprims);
  for (uint8_t k = 0; k < numprims; ++k) {
    if (p + 2 > size) return false;
    const uint8_t type = data[p];
    if (type >= 16) return false;
    const uint8_t count = kPrimIdxCount[type];
    const size_t body = 2 + static_cast<size_t>(numpaints) + count;
    if (p + body > size) return false;

    ShapePrimitive prim;
    prim.type = type;
    prim.flags = data[p + 1];
    const uint8_t pt = kPrimType[type];
    prim.kind = (pt == 1) ? PrimKind::Line : (pt == 5) ? PrimKind::Point : PrimKind::Polygon;
    prim.paint.assign(data + p + 2, data + p + 2 + numpaints);
    prim.indices.assign(data + p + 2 + numpaints, data + p + 2 + numpaints + count);
    // Validate indices.
    for (uint8_t idx : prim.indices) {
      if (idx >= numverts) return false;
    }
    shape.primitives.push_back(std::move(prim));
    p += body;
  }

  *out = std::move(shape);
  return true;
}

bool ShapeLibrary::load_p3s(const std::string& path) {
  std::vector<uint8_t> raw;
  if (!decompress_file(path, &raw)) {
    S3D_ERROR("shape3d: cannot decompress '%s'", path.c_str());
    return false;
  }
  ResFile res;
  if (!res.load(raw.data(), raw.size())) {
    S3D_ERROR("shape3d: '%s' is not a resource archive", path.c_str());
    return false;
  }

  size_t loaded = 0;
  for (const ResFile::Chunk& c : res.chunks()) {
    if (by_name_.count(c.id) != 0) continue;  // earlier archive wins
    Shape3D shape;
    const size_t off = res.data_offset() + c.offset;
    if (!parse_shape(raw.data(), raw.size(), off, &shape)) {
      S3D_WARN("shape3d: skipping malformed shape '%s' in '%s'", c.id.c_str(),
               path.c_str());
      continue;
    }
    shape.name = c.id;
    by_name_[c.id] = shapes_.size();
    shapes_.push_back(std::move(shape));
    ++loaded;
  }
  S3D_INFO("shape3d: loaded %zu shapes from '%s'", loaded, path.c_str());
  return true;
}

const Shape3D* ShapeLibrary::find(const std::string& name) const {
  auto it = by_name_.find(name);
  return (it == by_name_.end()) ? nullptr : &shapes_[it->second];
}

size_t ShapeLibrary::track_shape_count() {
  return sizeof(kTrackShapeNames) / sizeof(kTrackShapeNames[0]);
}

const char* ShapeLibrary::track_shape_name(size_t index) {
  return (index < track_shape_count()) ? kTrackShapeNames[index] : "";
}

}  // namespace s3d
