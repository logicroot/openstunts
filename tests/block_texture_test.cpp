// Procedural block-texture tests. The generator is deterministic and pixel
// based, so it can be checked without a GL context.

#include "ui/gui.h"

#include <algorithm>

#include "tiny_test.h"

namespace {

bool same_bytes(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

}  // namespace

TEST(block_texture_has_rgba_size) {
  std::vector<uint8_t> px;
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kDirt, 32, 1u, &px);
  EXPECT_EQ(px.size(), static_cast<size_t>(32) * 32 * 4);
}

TEST(block_texture_is_opaque) {
  std::vector<uint8_t> px;
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kStone, 16, 7u, &px);
  bool opaque = true;
  for (size_t i = 3; i < px.size(); i += 4) {
    if (px[i] != 255) opaque = false;
  }
  EXPECT_TRUE(opaque);
}

TEST(block_texture_is_deterministic) {
  std::vector<uint8_t> a;
  std::vector<uint8_t> b;
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kDirt, 24, 12345u, &a);
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kDirt, 24, 12345u, &b);
  EXPECT_TRUE(same_bytes(a, b));
}

TEST(block_texture_seed_and_kind_change_output) {
  std::vector<uint8_t> base;
  std::vector<uint8_t> other_seed;
  std::vector<uint8_t> other_kind;
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kDirt, 24, 1u, &base);
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kDirt, 24, 2u, &other_seed);
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kStone, 24, 1u, &other_kind);
  EXPECT_TRUE(!same_bytes(base, other_seed));
  EXPECT_TRUE(!same_bytes(base, other_kind));
}

TEST(block_texture_dirt_reads_warm) {
  // Dirt/grass tiles should at least be *tinted* per kind: check the dirt base
  // is warmer (more red than blue) than stone's neutral grey.
  std::vector<uint8_t> dirt;
  std::vector<uint8_t> stone;
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kDirt, 32, 5u, &dirt);
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kStone, 32, 5u, &stone);
  int dirt_rb = 0;
  int stone_rb = 0;
  const size_t pixels = 32 * 32;
  for (size_t p = 0; p < pixels; ++p) {
    dirt_rb += static_cast<int>(dirt[p * 4 + 0]) - static_cast<int>(dirt[p * 4 + 2]);
    stone_rb += static_cast<int>(stone[p * 4 + 0]) - static_cast<int>(stone[p * 4 + 2]);
  }
  EXPECT_TRUE(dirt_rb > stone_rb);
}

TEST(block_texture_tile_is_not_flat) {
  // A flat tile would read as a solid colour; the generator must vary it.
  std::vector<uint8_t> px;
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kDirt, 32, 9u, &px);
  const uint8_t first = px[0];
  bool varied = false;
  for (size_t p = 0; p < 32 * 32; ++p) {
    if (px[p * 4 + 0] != first || px[p * 4 + 1] != px[1]) varied = true;
  }
  EXPECT_TRUE(varied);
}

TEST(block_texture_zero_size_is_empty) {
  std::vector<uint8_t> px(4, 7);
  s3d::ui::generate_block_texture(s3d::ui::BlockTexture::kDirt, 0, 1u, &px);
  EXPECT_TRUE(px.empty());
}
