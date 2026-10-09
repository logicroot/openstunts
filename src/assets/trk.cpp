#include "assets/trk.h"

#include <cstdio>
#include <fstream>
#include <vector>

#include "assets/binary_reader.h"
#include "core/log.h"

namespace s3d {

Track Track::parse(const uint8_t* data, size_t size) {
  Track t;
  if (data == nullptr || size < kFileSize) {
    S3D_ERROR("TRK: file too small (%zu bytes, expected %zu)", size, kFileSize);
    return t;
  }
  ByteReader r(data, size);

  // Item grid, stored top->bottom already.
  for (size_t i = 0; i < kCount; ++i) t.items[i] = r.u8();

  t.sky = r.u8();

  // Terrain grid is stored bottom->top; flip into our top->bottom convention.
  for (int y = kSize - 1; y >= 0; --y) {
    for (int x = 0; x < kSize; ++x) {
      t.terrain[static_cast<size_t>(y) * kSize + x] = r.u8();
    }
  }
  t.unknown_tail = r.u8();
  return t;
}

Track Track::load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    S3D_ERROR("TRK: cannot open '%s'", path.c_str());
    return {};
  }
  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
  return parse(buf.data(), buf.size());
}

const char* Track::skybox_name() const {
  switch (sky) {
    case 0: return "desert";
    case 1: return "tropical";
    case 2: return "alpine";
    case 3: return "city";
    case 4: return "country";
    default: return "unknown";
  }
}

}  // namespace s3d
