#include "world/skybox.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/log.h"
#include "core/paths.h"

namespace s3d::world {

const char* const kSkyboxLayerIds[kSkyboxLayerCount] = {"scen", "sce2", "sce3", "sce4"};
const int kSkyboxLayerOffsets[kSkyboxLayerCount] = {0, 320, 512, 832};

int Skybox::min_height() const {
  int m = 0;
  bool first = true;
  for (const SkyboxLayer& l : layers) {
    if (!l.valid()) continue;
    if (first || l.sprite.height < m) {
      m = l.sprite.height;
      first = false;
    }
  }
  return m;
}

int Skybox::max_height() const {
  int m = 0;
  for (const SkyboxLayer& l : layers) {
    if (l.valid()) m = std::max(m, l.sprite.height);
  }
  return m;
}

bool load_skybox(const std::string& asset_dir, const std::string& theme, Skybox* out) {
  if (out == nullptr) return false;
  *out = Skybox{};
  if (asset_dir.empty() || theme.empty() || theme == "unknown") return false;

  std::string name = theme;
  for (char& c : name) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  }
  const std::string pvs = path_join(asset_dir, name + ".PVS");

  bool all = true;
  for (int i = 0; i < kSkyboxLayerCount; ++i) {
    SkyboxLayer& layer = out->layers[static_cast<size_t>(i)];
    layer.offset_x = kSkyboxLayerOffsets[i];
    if (!load_sprite2d(pvs, kSkyboxLayerIds[i], &layer.sprite)) {
      all = false;
      continue;
    }
    S3D_INFO("skybox: %s.PVS/%s %dx%d", name.c_str(), kSkyboxLayerIds[i],
             layer.sprite.width, layer.sprite.height);
  }
  out->loaded = all;
  if (!all) {
    S3D_WARN("skybox: %s.PVS is missing one or more scenery layers", name.c_str());
  }
  return all;
}

int skybox_layout(const Skybox& skybox, int view_width, int view_height,
                  float horizon_y, int yaw_units, SkyboxPlacement* out,
                  int max_out) {
  if (!skybox.valid() || view_width <= 0 || view_height <= 0 || out == nullptr ||
      max_out <= 0) {
    return 0;
  }

  // The original is a 320x200 view; scale the panorama with the view height so
  // the sprite pixels stay square (the same convention the HUD uses).
  const float scale = static_cast<float>(view_height) / 200.0f;
  const float period = static_cast<float>(kSkyboxPanoramaWidth) * scale;
  // skybox_op_helper2: si = ((yaw + 0x200) & 0x3FF) - 0x400. Working modulo the
  // panorama period, si is equivalent to (yaw + 0x200) & 0x3FF.
  const float u = static_cast<float>((yaw_units + 0x200) & 0x3FF) * scale;

  const int k_max = static_cast<int>(std::ceil(static_cast<float>(view_width) / period)) + 2;

  int count = 0;
  // The original lays the four layers at si + {0, 0x140, 0x200, 0x340} and
  // repeats the first at si + 0x400. Working modulo the 1024-wide panorama that
  // is just a seamless tiling, so every layer is placed at base + k*period; the
  // first layer's repeat is the k = +1 instance.
  for (int layer = 0; layer < kSkyboxLayerCount; ++layer) {
    const SkyboxLayer& l = skybox.layers[static_cast<size_t>(layer)];
    if (!l.valid()) continue;

    const float width = static_cast<float>(l.sprite.width) * scale;
    const float height = static_cast<float>(l.sprite.height) * scale;
    const float y = horizon_y - height;

    float base = std::fmod(u + static_cast<float>(l.offset_x) * scale, period);
    if (base < 0.0f) base += period;

    for (int k = -2; k <= k_max; ++k) {
      const float x = base + static_cast<float>(k) * period;
      if (x + width <= 0.0f || x >= static_cast<float>(view_width)) continue;
      if (count >= max_out) return count;
      SkyboxPlacement& p = out[count++];
      p.x = x;
      p.y = y;
      p.width = width;
      p.height = height;
      p.layer = layer;
    }
  }
  return count;
}

}  // namespace s3d::world
