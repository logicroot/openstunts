// s3d_dump: inspect original Stunts assets and print/convert them.
//
// Usage:
//   s3d_dump trk <file.trk>        ASCII item map + terrain map + statistics
//   s3d_dump dir                   show detected asset directory

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assets/compression.h"
#include "assets/res_file.h"
#include "assets/trk.h"
#include "core/log.h"
#include "core/paths.h"

using namespace s3d;

namespace {

char item_char(uint8_t v) {
  if (v == 0) return '.';
  if (v >= 0x04 && v <= 0x0d) return 'R';  // normal road
  if (v >= 0x0e && v <= 0x17) return 'd';  // dirty road
  if (v >= 0x18 && v <= 0x21) return 'i';  // icy road
  if (v >= 0x22 && v <= 0x27) return '^';  // raised road / ramps
  if (v >= 0x38 && v <= 0x3b) return 'B';  // bridges
  return '?';
}

char terrain_char(uint8_t v) {
  if (v == 0x00) return '.';
  if (v == 0x01) return '~';
  if (v >= 0x02 && v <= 0x05) return '/';
  if (v == 0x06) return '#';
  if (v >= 0x07 && v <= 0x12) return '\\';
  return '?';
}

void dump_trk(const std::string& path) {
  Track t = Track::load(path);
  std::printf("--- items (top to bottom) ---\n");
  for (int y = 0; y < Track::kSize; ++y) {
    for (int x = 0; x < Track::kSize; ++x) std::putchar(item_char(t.item(x, y)));
    std::putchar('\n');
  }
  std::printf("--- terrain (top to bottom) ---\n");
  for (int y = 0; y < Track::kSize; ++y) {
    for (int x = 0; x < Track::kSize; ++x) std::putchar(terrain_char(t.terrain_at(x, y)));
    std::putchar('\n');
  }
  std::printf("skybox: %s (%u), unknown tail byte: 0x%02x\n", t.skybox_name(), t.sky,
              t.unknown_tail);
}

void dump_p3s(const std::string& path) {
  std::vector<uint8_t> raw;
  if (!decompress_file(path, &raw)) {
    std::fprintf(stderr, "cannot decompress '%s'\n", path.c_str());
    return;
  }
  ResFile res;
  if (!res.load(raw.data(), raw.size())) {
    std::fprintf(stderr, "'%s' is not a resource archive\n", path.c_str());
    return;
  }
  std::printf("%s: %zu bytes decompressed, %zu resources\n", path.c_str(), raw.size(),
              res.chunks().size());
  for (const ResFile::Chunk& c : res.chunks()) {
    std::printf("  %-8s %u\n", c.id.c_str(), c.size);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr,
                 "usage:\n"
                 "  s3d_dump trk <file.trk>\n"
                 "  s3d_dump p3s <file.p3s>\n"
                 "  s3d_dump dir\n");
    return 2;
  }
  const std::string cmd = argv[1];
  if (cmd == "trk" && argc >= 3) {
    dump_trk(argv[2]);
    return 0;
  }
  if (cmd == "dir") {
    const std::string dir = find_asset_dir();
    std::printf("%s\n", dir.empty() ? "(not found)" : dir.c_str());
    return dir.empty() ? 1 : 0;
  }
  if (cmd == "p3s" && argc >= 3) {
    dump_p3s(argv[2]);
    return 0;
  }
  std::fprintf(stderr, "unknown command: %s\n", cmd.c_str());
  return 2;
}
