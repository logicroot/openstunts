#pragma once
// Minecraft-styled 2D GUI for the front-end.
//
// Everything is built into screen-space quads and drawn through the existing
// HUD (flat, alpha-blended) and sprite (textured) renderers, so the GUI composes
// over a 3D background (the attract-mode demo behind the main menu). Text uses
// the original FONTDEF.FNT bitmap font; the block textures are generated in
// memory so the front-end needs no new asset files.

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "assets/font.h"
#include "render/hud.h"
#include "render/mesh.h"
#include "render/sprite_renderer.h"
#include "render/texture.h"

namespace s3d::ui {

// Which procedural block texture to generate.
enum class BlockTexture { kDirt, kStone, kGrass, kSand };

// Resolution of the generated block tiles and the size, in logical GUI pixels,
// one tile covers on screen. Shared with the front-end, which tiles them itself
// behind a GUI overlay.
inline constexpr int kBlockTextureSize = 64;
inline constexpr float kBlockTilePixels = 96.0f;
inline constexpr uint32_t kBlockSeed = 0x5354554eu;  // "STUN"

// Fills `pixels` with a deterministic, tileable `size`x`size` RGBA block
// texture. The result is fully opaque; the same kind + seed always yields the
// same bytes, so callers (and tests) can rely on it.
void generate_block_texture(BlockTexture kind, int size, uint32_t seed,
                            std::vector<uint8_t>* pixels);

// Colours and metrics of the Minecraft-style widgets. All sizes are in logical
// pixels at the GUI's built-in scale (see Gui::set_scale).
struct GuiTheme {
  float button_height = 36.0f;
  float button_gap = 6.0f;
  float bevel = 3.0f;  // bevel/border thickness in pixels
  float padding = 16.0f;

  glm::vec4 scrim{0.0f, 0.0f, 0.0f, 0.55f};
  glm::vec4 panel{0.06f, 0.06f, 0.08f, 0.82f};
  glm::vec4 panel_border{0.0f, 0.0f, 0.0f, 0.9f};
  glm::vec4 button{0.55f, 0.55f, 0.55f, 1.0f};
  glm::vec4 button_focus{0.74f, 0.78f, 0.88f, 1.0f};
  glm::vec4 button_disabled{0.28f, 0.28f, 0.28f, 1.0f};
  glm::vec4 bevel_light{1.0f, 1.0f, 1.0f, 0.55f};
  glm::vec4 bevel_dark{0.0f, 0.0f, 0.0f, 0.55f};
  glm::vec4 focus_border{1.0f, 1.0f, 1.0f, 1.0f};
  glm::vec4 text{0.96f, 0.96f, 0.96f, 1.0f};
  glm::vec4 text_disabled{0.52f, 0.52f, 0.52f, 1.0f};
  glm::vec4 text_shadow{0.15f, 0.15f, 0.15f, 0.85f};
  glm::vec4 title{1.0f, 0.85f, 0.28f, 1.0f};  // yellow, Minecraft splash
  glm::vec4 title_shadow{0.22f, 0.13f, 0.0f, 1.0f};
};

class Gui {
 public:
  Gui() = default;

  // Loads FONTDEF.FNT from `asset_dir` (optional: without it text is skipped)
  // and generates the block textures. Safe to call in headless tests.
  bool init(const std::string& asset_dir);
  void shutdown();

  GuiTheme& theme() { return theme_; }
  const GuiTheme& theme() const { return theme_; }

  // Global pixel scale for every widget (the front-end scales by resolution).
  void set_scale(float scale) { scale_ = scale; }
  float scale() const { return scale_; }

  const BitmapFont& font() const { return font_; }
  bool has_font() const { return font_.valid(); }

  // The sprite renderer the GUI sits on; exposed so a screen can draw an asset
  // image (e.g. an opponent face) behind the menu overlay.
  SpriteRenderer& sprites() { return sprites_; }

  // Resets the per-frame draw list. `drawable_width/height` are the current
  // framebuffer size in pixels; the GUI works in `scale`-scaled units.
  void begin(int drawable_width, int drawable_height);
  int view_width() const { return view_w_; }
  int view_height() const { return view_h_; }

  // Dim overlay across the whole view.
  void scrim();
  // A raised panel with a border.
  void panel(float x, float y, float w, float h);

  // A Minecraft-style button. `focused` adds the bright fill and white border;
  // `enabled` false dims both the fill and the label.
  void button(float x, float y, float w, float h, const std::string& label,
              bool focused, bool enabled = true);
  // A button with a left-aligned label and a right-aligned value (used by value
  // rows such as Resolution and Transmission). `focus_marker` draws a ">  <"
  // pair around the value.
  void value_button(float x, float y, float w, float h, const std::string& label,
                    const std::string& value, bool focused, bool enabled = true);
  // A dimmed, unselectable informational row.
  void notice(float x, float y, float w, float h, const std::string& label);

  // Text helpers (top-left anchored unless noted).
  void text(const std::string& t, float x, float y, const glm::vec4& color);
  void text_shadowed(const std::string& t, float x, float y, const glm::vec4& color);
  void text_centered(const std::string& t, float cx, float y, const glm::vec4& color);
  void title_centered(const std::string& t, float cx, float y);
  float measure(const std::string& t) const;

  // Flushes the accumulated quads: the textured background first, then the flat
  // (blended) layer with panels, buttons and text.
  void flush();

 private:
  void quad(float x, float y, float w, float h, const glm::vec4& color);
  // Nearest-neighbour font pixel scale and one text line's pixel height.
  float text_px() const;
  float line_height() const;

  float scale_ = 1.0f;
  int drawable_w_ = 0;
  int drawable_h_ = 0;
  int view_w_ = 0;
  int view_h_ = 0;

  GuiTheme theme_;
  BitmapFont font_;
  HudRenderer hud_;
  SpriteRenderer sprites_;
  std::vector<Vertex> flat_;
};

}  // namespace s3d::ui
