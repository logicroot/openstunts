#pragma once
// Scenery skybox: the horizon panorama drawn behind the track.
//
// The original loads a per-theme shape2d archive named after the track's skybox
// id (TRK byte 900, see Track::skybox_name: 0=desert 1=tropical 2=alpine
// 3=city 4=country) and locates four 2D-shape "scenery" bitmaps under the
// resource ids "scen", "sce2", "sce3" and "sce4". Laid end to end they form a
// seamless 1024-pixel-wide mountain panorama:
//
//   scen [0,320)   sce2 [320,512)   sce3 [512,832)   sce4 [832,1024)
//
// Each layer stands on the horizon line and is stored with its own height (the
// SHAPE2D.s2d_height that load_skybox caches in skybox_ptr1..4). The panorama
// scrolls one pixel per 1/1024-turn of camera yaw; skybox_op_helper2 fills the
// sky down to the horizon minus the shortest layer, blits the four layers
// opaquely and repeats the first layer at +0x400 so the 1024-wide strip tiles.
//
// Reference: seg003.asm load_skybox (6084) and skybox_op_helper2 (3792).

#include <array>
#include <cstdint>
#include <string>

#include "assets/sprite2d.h"

namespace s3d::world {

inline constexpr int kSkyboxLayerCount = 4;
inline constexpr int kSkyboxPanoramaWidth = 1024;

// The layer resource ids ("scen"/"sce2"/"sce3"/"sce4") and their left edges in
// panorama pixels, in draw order.
extern const char* const kSkyboxLayerIds[kSkyboxLayerCount];
extern const int kSkyboxLayerOffsets[kSkyboxLayerCount];

struct SkyboxLayer {
  Sprite2D sprite;   // width/height pixels + palette indices (0xFF transparent)
  int offset_x = 0;  // left edge in panorama pixels

  bool valid() const { return sprite.valid(); }
};

struct Skybox {
  std::array<SkyboxLayer, kSkyboxLayerCount> layers{};
  bool loaded = false;

  bool valid() const { return loaded; }
  // SHAPE2D.s2d_height minima / maxima (skybox_current / word_454CE).
  int min_height() const;
  int max_height() const;
};

// Loads `<theme>.PVS` (theme is the lowercase name from Track::skybox_name)
// from `asset_dir` and its four scenery layers. Returns false when the archive
// or any layer is missing.
bool load_skybox(const std::string& asset_dir, const std::string& theme,
                 Skybox* out);

// One screen-space placement of a skybox layer, in top-left pixel coordinates.
struct SkyboxPlacement {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  int layer = 0;
};

// Computes the screen-space placements of the panorama layers for a viewport of
// `view_width` x `view_height` pixels, a horizon at `horizon_y` pixels and a
// camera yaw of `yaw_units` 1/1024-turns. The panorama scrolls one (scaled)
// pixel per yaw unit and each layer's top is `height * scale` above the
// horizon, where scale = view_height / 200 (the original is 320x200). At most
// `max_out` placements are written; the number written is returned.
//
// Follows skybox_op_helper2 (seg003.asm 3942): si = (yaw + 0x200) & 0x3FF, the
// layers sit at panorama x = si + {0, 0x140, 0x200, 0x340} plus a repeat of the
// first at si + 0x400.
int skybox_layout(const Skybox& skybox, int view_width, int view_height,
                  float horizon_y, int yaw_units, SkyboxPlacement* out,
                  int max_out);

}  // namespace s3d::world
