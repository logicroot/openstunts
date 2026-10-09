#include "assets/palette.h"

#include <fstream>
#include <iterator>
#include <vector>

#include "assets/compression.h"
#include "assets/res_file.h"
#include "core/log.h"

namespace s3d {

void Palette::set_from_dac(const uint8_t* dac768) {
  for (int i = 0; i < 256; ++i) {
    for (int c = 0; c < 3; ++c) {
      const uint8_t v = dac768[i * 3 + c] & 0x3f;
      raw6[i][c] = v;
      rgb8[i][c] = static_cast<uint8_t>((v << 2) | (v >> 4));
    }
  }
}

namespace {

// `!pal` is a 2D shape with a 16-byte header followed by the 768-byte DAC table.
constexpr size_t kPaletteHeaderSize = 16;
constexpr size_t kPaletteDacSize = 768;

}  // namespace

bool load_vga_palette(const std::string& archive_path, Palette* out) {
  if (out == nullptr) return false;

  std::vector<uint8_t> raw;
  // Try the compressed container first (.PVS/.PRE/.PES/.COD); fall back to a
  // raw .RES archive.
  if (!decompress_file(archive_path, &raw)) {
    std::ifstream f(archive_path, std::ios::binary);
    if (!f) {
      S3D_ERROR("palette: cannot read '%s'", archive_path.c_str());
      return false;
    }
    raw.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  }

  ResFile res;
  if (!res.load(raw.data(), raw.size())) {
    S3D_ERROR("palette: '%s' is not a resource archive", archive_path.c_str());
    return false;
  }
  const ResFile::Chunk* pal = res.find("!pal");
  if (pal == nullptr) {
    S3D_ERROR("palette: '%s' has no '!pal' resource", archive_path.c_str());
    return false;
  }
  const size_t off = res.data_offset() + pal->offset;
  if (pal->size < kPaletteHeaderSize + kPaletteDacSize || off + kPaletteHeaderSize + kPaletteDacSize > raw.size()) {
    S3D_ERROR("palette: '!pal' in '%s' is too small", archive_path.c_str());
    return false;
  }
  out->set_from_dac(raw.data() + off + kPaletteHeaderSize);
  return true;
}

}  // namespace s3d
