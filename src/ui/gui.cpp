#include "ui/gui.h"

#include <algorithm>
#include <cmath>

#include "core/log.h"
#include "core/paths.h"

namespace s3d::ui {

namespace {

// Deterministic integer hash (splitmix-style finalizer). Used to drive the
// procedural block textures so a given seed always yields the same bytes.
uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return x;
}

// [0, 1) pseudo-random float advanced from `state`.
float frand(uint32_t* state) {
  *state = hash32(*state + 0x9e3779b9u);
  return static_cast<float>(*state >> 8) / 16777216.0f;
}

struct BlockBase {
  float r, g, b;
};

BlockBase block_base(BlockTexture kind) {
  switch (kind) {
    case BlockTexture::kStone: return {0.50f, 0.50f, 0.50f};
    case BlockTexture::kGrass: return {0.34f, 0.55f, 0.24f};
    case BlockTexture::kSand: return {0.85f, 0.80f, 0.63f};
    case BlockTexture::kDirt:
    default: return {0.53f, 0.38f, 0.26f};
  }
}

uint8_t to_byte(float v) {
  v = std::clamp(v, 0.0f, 1.0f);
  return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

}  // namespace

void generate_block_texture(BlockTexture kind, int size, uint32_t seed,
                            std::vector<uint8_t>* pixels) {
  if (pixels == nullptr) return;
  if (size <= 0) {
    pixels->clear();
    return;
  }
  pixels->assign(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);

  const BlockBase base = block_base(kind);
  uint32_t state = hash32(seed ^ (static_cast<uint32_t>(kind) * 0x85ebca6bu));
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      // Per-pixel value noise: a coarse brightness jitter plus sparse darker
      // "specks", which reads as the classic 16x16 Minecraft block tiles.
      const float n = frand(&state);
      float shade = 0.78f + 0.40f * n;
      if (frand(&state) < 0.07f) shade *= 0.72f;
      const size_t p = (static_cast<size_t>(y) * size + x) * 4;
      (*pixels)[p + 0] = to_byte(base.r * shade);
      (*pixels)[p + 1] = to_byte(base.g * shade);
      (*pixels)[p + 2] = to_byte(base.b * shade);
      (*pixels)[p + 3] = 255;
    }
  }
}

bool Gui::init(const std::string& asset_dir) {
  if (!hud_.init()) S3D_WARN("gui: HUD renderer initialization failed");
  if (!sprites_.init()) S3D_WARN("gui: sprite renderer initialization failed");

  if (!asset_dir.empty() && font_.load(path_join(asset_dir, "FONTDEF.FNT"))) {
    S3D_INFO("gui: FONTDEF.FNT loaded (%d rows)", font_.rows());
  } else {
    S3D_WARN("gui: FONTDEF.FNT not found; menu text will be blank");
  }
  return true;
}

void Gui::shutdown() {
  hud_.shutdown();
  sprites_.shutdown();
}

void Gui::begin(int drawable_width, int drawable_height) {
  drawable_w_ = drawable_width;
  drawable_h_ = drawable_height;
  view_w_ = static_cast<int>(std::lround(static_cast<float>(drawable_width) / scale_));
  view_h_ = static_cast<int>(std::lround(static_cast<float>(drawable_height) / scale_));
  flat_.clear();
}

float Gui::text_px() const {
  // Scale the bitmap font to roughly 16 logical pixels tall, but never below a
  // 2x nearest-neighbour blit so it keeps an integer pixel look.
  const int rows = font_.valid() ? font_.rows() : 8;
  const float target = 16.0f;
  return std::max(2.0f, std::round(target / static_cast<float>(rows)));
}

float Gui::line_height() const {
  const int rows = font_.valid() ? font_.rows() : 8;
  return static_cast<float>(rows) * text_px();
}

void Gui::quad(float x, float y, float w, float h, const glm::vec4& color) {
  Vertex v;
  v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  v.color = color;
  const float sx = (x + w) * scale_;
  const float sy = (y + h) * scale_;
  const float px = x * scale_;
  const float py = y * scale_;
  const glm::vec3 pts[6] = {{px, py, 0.0f}, {sx, py, 0.0f}, {sx, sy, 0.0f},
                            {px, py, 0.0f}, {sx, sy, 0.0f}, {px, sy, 0.0f}};
  for (const glm::vec3& p : pts) {
    v.position = p;
    flat_.push_back(v);
  }
}

void Gui::scrim() {
  quad(0.0f, 0.0f, static_cast<float>(view_w_), static_cast<float>(view_h_), theme_.scrim);
}

void Gui::panel(float x, float y, float w, float h) {
  quad(x, y, w, h, theme_.panel);
  const float b = std::max(1.0f, theme_.bevel);
  quad(x - b, y - b, w + 2 * b, b, theme_.panel_border);
  quad(x - b, y + h, w + 2 * b, b, theme_.panel_border);
  quad(x - b, y, b, h, theme_.panel_border);
  quad(x + w, y, b, h, theme_.panel_border);
}

void Gui::button(float x, float y, float w, float h, const std::string& label,
                 bool focused, bool enabled) {
  const glm::vec4 fill = !enabled ? theme_.button_disabled
                                  : (focused ? theme_.button_focus : theme_.button);
  quad(x, y, w, h, fill);

  const float b = std::max(1.0f, theme_.bevel);
  quad(x, y, w, b, theme_.bevel_light);
  quad(x, y, b, h, theme_.bevel_light);
  quad(x, y + h - b, w, b, theme_.bevel_dark);
  quad(x + w - b, y, b, h, theme_.bevel_dark);
  if (focused && enabled) {
    quad(x, y, w, b, theme_.focus_border);
    quad(x, y, b, h, theme_.focus_border);
    quad(x, y + h - b, w, b, theme_.focus_border);
    quad(x + w - b, y, b, h, theme_.focus_border);
  }

  const glm::vec4 color = enabled ? theme_.text : theme_.text_disabled;
  const float tw = measure(label);
  const float tx = x + (w - tw) * 0.5f;
  const float ty = y + (h - line_height()) * 0.5f;
  text_shadowed(label, tx, ty, color);
}

void Gui::value_button(float x, float y, float w, float h, const std::string& label,
                       const std::string& value, bool focused, bool enabled) {
  button(x, y, w, h, "", focused, enabled);
  const glm::vec4 color = enabled ? theme_.text : theme_.text_disabled;
  const float pad = theme_.padding * 0.5f;
  const float ty = y + (h - line_height()) * 0.5f;
  text_shadowed(label, x + pad, ty, color);
  const float vw = measure(value);
  const float vx = x + w - pad - vw;
  text_shadowed(value, vx, ty, color);
  if (focused && enabled) {
    const float marker_ty = ty;
    text_shadowed("<", vx - measure("< ") - 2.0f, marker_ty, theme_.title);
    text_shadowed(">", x + w - pad + 2.0f, marker_ty, theme_.title);
  }
}

void Gui::notice(float x, float y, float w, float h, const std::string& label) {
  const float tw = measure(label);
  text_shadowed(label, x + (w - tw) * 0.5f, y + (h - line_height()) * 0.5f,
                theme_.text_disabled);
}

void Gui::text(const std::string& t, float x, float y, const glm::vec4& color) {
  if (!font_.valid()) return;
  append_text(flat_, font_, t, x * scale_, y * scale_, text_px() * scale_, color);
}

void Gui::text_shadowed(const std::string& t, float x, float y, const glm::vec4& color) {
  if (!font_.valid()) return;
  text(t, x + 1.0f, y + 1.0f, theme_.text_shadow);
  text(t, x, y, color);
}

void Gui::text_centered(const std::string& t, float cx, float y, const glm::vec4& color) {
  text_shadowed(t, cx - measure(t) * 0.5f, y, color);
}

void Gui::title_centered(const std::string& t, float cx, float y) {
  if (!font_.valid()) return;
  // Title text is a 2x blit of the normal text scale.
  const float ts = text_px() * 2.0f;
  const float w = static_cast<float>(font_.text_width(t)) * ts;
  const float x = (cx * scale_) - w * 0.5f;
  const float px = x + 2.0f * scale_;
  const float py = y * scale_ + 2.0f * scale_;
  append_text(flat_, font_, t, px, py, ts * scale_, theme_.title_shadow);
  append_text(flat_, font_, t, x, y * scale_, ts * scale_, theme_.title);
}

float Gui::measure(const std::string& t) const {
  if (!font_.valid()) return 0.0f;
  return static_cast<float>(font_.text_width(t)) * text_px();
}

void Gui::flush() {
  if (drawable_w_ <= 0 || drawable_h_ <= 0) return;
  hud_.draw(flat_, drawable_w_, drawable_h_, true);
}

}  // namespace s3d::ui
