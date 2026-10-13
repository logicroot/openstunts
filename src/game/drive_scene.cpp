#include "game/drive_scene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "assets/palette.h"
#include "core/log.h"
#include "core/paths.h"
#include "physics/car_engine.h"
#include "physics/car_motion.h"
#include "physics/car_steering.h"
#include "physics/fixed_math.h"
#include "physics/wrap_math.h"
#include "platform/input.h"
#include "render/renderer.h"
#include "render/screenshot.h"
#include "world/car_mesh.h"
#include "world/material.h"
#include "world/mesh_builder.h"
#include "world/shape_mesh.h"
#include "world/track_elevation.h"
#include "world/track_mesh.h"
#include "world/track_shapes.h"
#include "world/world_units.h"

namespace s3d {

namespace {

constexpr double kPhysicsDt = 1.0 / 20.0;
constexpr int kMaxPhysicsStepsPerFrame = 4;
constexpr float kCarBodyDrop = 6.0f;  // pivot rests 6 above the wheel contact plane

// Follow-camera rates (restunts gamestep.c), per 20 Hz physics tick.
constexpr float kFollowDistance = 450.0f;
constexpr float kFollowHeight = 270.0f;
constexpr float kFollowAimHeight = 60.0f;
constexpr float kFollowHorizontalStep = 120.0f;
constexpr float kFollowVerticalStep = 30.0f;

constexpr float kAngleToRadians = 6.28318530718f / 1024.0f;

// The port drives the Countach (gameconfig.game_playercarid default "COUN"): its
// performance data is CARCOUN.RES, its bodywork STCOUN.P3S and its cockpit
// dashboard STDACOUN.PVS.
constexpr const char* kPlayerCarId = "COUN";

// 8-bit RGB for a VGA palette index (the original draws HUD text with the
// palette indices 15 = white and 0 = black drop shadow).
glm::vec4 palette_color(const Palette& p, int index) {
  const uint8_t* c = p.rgb8[index & 0xFF];
  return glm::vec4(static_cast<float>(c[0]) / 255.0f, static_cast<float>(c[1]) / 255.0f,
                   static_cast<float>(c[2]) / 255.0f, 1.0f);
}

// Appends one solid axis-aligned quad (two triangles) to the HUD vertices.
void append_quad(std::vector<Vertex>& out, float x, float y, float w, float h,
                 const glm::vec4& color) {
  Vertex v;
  v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  v.color = color;
  const float sx = x + w;
  const float sy = y + h;
  const glm::vec3 pts[6] = {{x, y, 0.0f},  {sx, y, 0.0f},  {sx, sy, 0.0f},
                            {x, y, 0.0f},  {sx, sy, 0.0f}, {x, sy, 0.0f}};
  for (const glm::vec3& p : pts) {
    v.position = p;
    out.push_back(v);
  }
}

// Draws a 2D sprite as one `scale`x`scale` quad per opaque pixel. Index 0xFF
// (and any other non-palette value) is the transparent colour key
// (seg012.asm sprite_putimage_transparent via the `incnums` table).
void append_sprite(std::vector<Vertex>& out, const Sprite2D& s, const Palette& pal,
                   float x, float y, float scale) {
  if (!s.valid()) return;
  for (int py = 0; py < s.height; ++py) {
    for (int px = 0; px < s.width; ++px) {
      const uint8_t idx = s.pixels[static_cast<size_t>(py) * s.width + px];
      if (idx >= 16) continue;
      append_quad(out, x + static_cast<float>(px) * scale,
                  y + static_cast<float>(py) * scale, scale, scale,
                  palette_color(pal, idx));
    }
  }
}

// Appends one line segment as a thin quad (perpendicular thickness `thickness`)
// for the crashed-windscreen overlay.
void append_segment(std::vector<Vertex>& out, float x1, float y1, float x2, float y2,
                    float thickness, const glm::vec4& color) {
  glm::vec2 a(x1, y1), b(x2, y2);
  glm::vec2 d = b - a;
  const float len = glm::length(d);
  if (len < 1e-4f) {
    d = glm::vec2(1.0f, 0.0f);
  } else {
    d /= len;
  }
  const glm::vec2 n(-d.y, d.x);
  const glm::vec2 o = n * (thickness * 0.5f);
  Vertex v;
  v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  v.color = color;
  const glm::vec3 pts[6] = {{a.x - o.x, a.y - o.y, 0.0f}, {a.x + o.x, a.y + o.y, 0.0f},
                            {b.x + o.x, b.y + o.y, 0.0f}, {a.x - o.x, a.y - o.y, 0.0f},
                            {b.x + o.x, b.y + o.y, 0.0f}, {b.x - o.x, b.y - o.y, 0.0f}};
  for (const glm::vec3& p : pts) {
    v.position = p;
    out.push_back(v);
  }
}

// Appends one axis-aligned quad sampling the full [0,1] uv range of a texture.
void append_textured_quad(std::vector<Vertex>& out, float x, float y, float w,
                          float h) {
  Vertex v;
  v.normal = glm::vec3(0.0f, 0.0f, 1.0f);
  v.color = glm::vec4(1.0f);
  const float sx = x + w;
  const float sy = y + h;
  const glm::vec3 p[6] = {{x, y, 0.0f},  {sx, y, 0.0f},  {sx, sy, 0.0f},
                          {x, y, 0.0f},  {sx, sy, 0.0f}, {x, sy, 0.0f}};
  const glm::vec2 uv[6] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
                           {0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  for (int i = 0; i < 6; ++i) {
    v.position = p[i];
    v.uv = uv[i];
    out.push_back(v);
  }
}

void aim_at(Camera& cam, const glm::vec3& target) {
  const glm::vec3 d = glm::normalize(target - cam.position);
  cam.yaw_deg = glm::degrees(std::atan2(d.z, d.x));
  cam.pitch_deg = glm::degrees(std::asin(d.y));
}

// Placeholder torque curve for asset-less runs; index = rpm >> 7.
void fill_fallback_torque_curve(CarSpec* spec) {
  spec->idle_torque = 100;
  for (int i = 0; i < 104; ++i) {
    const int rpm = i << 7;
    int16_t t = 95;
    if (rpm < 2000) t = 95 + rpm / 64;
    else if (rpm < 5500) t = 110 - (rpm - 2000) / 300;
    else t = std::max(60, 104 - (rpm - 5500) / 120);
    spec->torque_curve[i] = static_cast<uint8_t>(std::clamp(static_cast<int>(t), 0, 255));
  }
}

CarSpec load_or_default_spec(const std::string& asset_dir) {
  CarSpec spec;
  const std::string car_res = std::string("CAR") + kPlayerCarId + ".RES";
  if (!asset_dir.empty() && CarSpec::load(path_join(asset_dir, car_res), &spec)) {
    return spec;
  }
  S3D_WARN("%s not found; using a placeholder car specification", car_res.c_str());

  // Counter-exact physical constants (verified against CARCOUN.RES) so
  // steering/grip feel right even without the retail asset file.
  spec.num_gears = 6;
  spec.car_mass = 33;
  spec.braking_eff = 200;
  spec.idle_rpm = 800;
  spec.downshift_rpm = 5000;
  spec.upshift_rpm = 7800;
  spec.max_rpm = 8250;
  const uint16_t ratios[7] = {0, 33339, 23194, 16516, 12086, 8287, 0};
  for (int i = 0; i < 7; ++i) spec.gear_ratios[i] = ratios[i];
  spec.aero_resistance = 25;
  for (int i = 0; i < 64; ++i) {
    spec.aero_resistance_table[i] = static_cast<int16_t>(
        static_cast<uint16_t>((static_cast<int32_t>(spec.aero_resistance) * i * i) >> 9));
  }
  spec.grip = 210;
  spec.sliding = 0;
  spec.surface_grip[0] = 256;  // paved
  spec.surface_grip[1] = 192;  // dirt
  spec.surface_grip[2] = 64;   // ice
  spec.surface_grip[3] = 128;  // grass
  spec.car_height = 16;
  const int16_t wheels[4][3] = {
      {-960, 0, 1984}, {960, 0, 1984}, {960, 0, -1984}, {-960, 0, -1984}};
  for (int i = 0; i < 4; ++i) {
    spec.wheel_coords[i].x = wheels[i][0];
    spec.wheel_coords[i].y = wheels[i][1];
    spec.wheel_coords[i].z = wheels[i][2];
  }
  fill_fallback_torque_curve(&spec);
  return spec;
}

// Ground callback feed to the physics: the shared grid surface model.
physics::GroundSample ground_sample(const Track& track, float wx, float wz) {
  const world::GroundSample g = world::ground_sample_at(track, wx, wz);
  physics::GroundSample out;
  out.height = g.height;
  out.surface = g.surface;
  return out;
}

// Linear interpolation between two 16-bit rotations. Per-tick deltas are small
// (well under a quarter turn), so no wraparound handling is needed.
float lerp_angle(int16_t prev, int16_t curr, float alpha) {
  return (static_cast<float>(prev) + (static_cast<float>(curr) - static_cast<float>(prev)) * alpha);
}

}  // namespace

DriveScene::DriveScene(Track track, std::string asset_dir)
    : track_(std::move(track)), asset_dir_(std::move(asset_dir)) {}

bool DriveScene::on_enter() {
  // ---- Static world geometry -----------------------------------------------
  ShapeLibrary lib;
  if (!asset_dir_.empty()) {
    lib.load_p3s(path_join(asset_dir_, "GAME1.P3S"));
    lib.load_p3s(path_join(asset_dir_, "GAME2.P3S"));
  }
  bool have_palette = false;
  if (!lib.shapes().empty()) {
    have_palette =
        !asset_dir_.empty() && load_vga_palette(path_join(asset_dir_, "SDMAIN.PVS"), &palette_);
    if (!have_palette) {
      S3D_WARN("SDMAIN.PVS palette not found; using fallback track colours");
    }

    // Static terrain and non-animated elements.
    world::TrackShapeStats stats;
    MeshBuilder mb;
    if (have_palette) {
      world::build_track_static_geometry(track_, lib, mb, palette_, &stats);
    } else {
      world::build_track_static_geometry(track_, lib, mb, &stats);
    }
    S3D_INFO("drive track: ground=%d terrain=%d elements=%d overlays=%d missing=%d",
             stats.ground_cells, stats.terrain_shapes, stats.elements, stats.overlays,
             stats.missing_shapes);
    track_mesh_ = mb.to_mesh();

    // Animated objects (windmill sails, Joe's flashing sign, ...), one mesh per
    // paint phase. The original cycles byte_3C0C6[game_frame & 15] over 0..3.
    world::TrackShapeStats anim_stats;
    for (int phase = 0; phase < world::kTrackAnimationPhases; ++phase) {
      MeshBuilder amb;
      MeshBuilder alb;
      if (have_palette) {
        world::build_track_animated_geometry(track_, lib, amb, palette_, phase, &anim_stats, &alb);
      } else {
        world::build_track_animated_geometry(track_, lib, amb, phase, &anim_stats, &alb);
      }
      animated_meshes_[phase] = amb.to_mesh();
      animated_lines_[phase] = alb.to_mesh();
    }
    S3D_INFO("drive track: %d animated objects (%d sign line verts)", anim_stats.animated,
             static_cast<int>(animated_lines_[0].vertex_count()));
  } else {
    S3D_WARN("no GAME1/GAME2 shapes found; falling back to placeholder geometry");
    track_mesh_ = build_track_mesh(track_);
  }

  // ---- Player car ----------------------------------------------------------
  spec_ = load_or_default_spec(asset_dir_);

  have_palette_ = have_palette;
  car_lib_ = std::make_shared<ShapeLibrary>();
  if (asset_dir_.empty() ||
      !world::load_car_archive(asset_dir_, kPlayerCarId, car_lib_.get())) {
    car_lib_.reset();
  }
  wheel_pose_ = world::CarWheelPose{};
  build_car_meshes();
  if (car_mesh_.empty()) {
    S3D_WARN("player car shapes not found; driving invisible");
  }

  // ---- Track collision -----------------------------------------------------
  // The element plane/wall tables live in GAME.PRE. With them bound to the
  // track grid the wheels ride ramps, banked roads and elevated terrain; absent
  // the file the motion code falls back to the flat 2D ground sampler.
  if (!asset_dir_.empty() &&
      collision_.load_from_game_pre(path_join(asset_dir_, "GAME.PRE"))) {
    collision_.set_track(&track_);
    S3D_INFO("track collision loaded: %zu planes, %zu walls", collision_.plane_count(),
             collision_.wall_count());
  } else {
    S3D_WARN("GAME.PRE not found; using the flat 2D ground sampler");
  }

  // ---- Race setup ----------------------------------------------------------
  // Resolve the start/finish line from the track's element grid (seg004.asm)
  // and load the LED font used for the on-screen race clock.
  start_finish_ = game::find_start_finish(track_);
  if (!start_finish_.found) {
    S3D_WARN("no start/finish element in the track; using the default spawn");
  } else if (start_finish_.multiple) {
    S3D_WARN("multiple start/finish elements; using the first at (%d,%d)",
             start_finish_.col, start_finish_.row);
  }

  // Walk the route graph (seg004.asm track_setup). The wrong-way / off-track
  // penalty scan and the finish gate (field_CD) need it; without it the run
  // cannot finish, matching the original's track setup errors.
  if (race_.build_route(track_, start_finish_)) {
    S3D_INFO("route: %d nodes from start (%d,%d)", race_.route.node_count(),
             start_finish_.col, 29 - start_finish_.row);
  } else if (start_finish_.found) {
    S3D_WARN("route setup failed: %s",
             world::route_error_name(race_.route.error()));
  }

  if (!asset_dir_.empty() && hud_font_.load(path_join(asset_dir_, "FONTLED.FNT"))) {
    S3D_INFO("race clock font FONTLED.FNT loaded (%d rows)", hud_font_.rows());
  } else {
    S3D_WARN("FONTLED.FNT not found; the race clock HUD is disabled");
  }
  // The warning banner and the direction arrows (seg003.asm draw_ingame_text).
  // The in-game text uses FONTDEF.FNT (frame.c font_set_fontdef); the arrows are
  // the SDGAME2.PVS "left"/"righ" sprites at screen centre.
  if (!asset_dir_.empty()) {
    if (text_font_.load(path_join(asset_dir_, "FONTDEF.FNT"))) {
      S3D_INFO("banner font FONTDEF.FNT loaded (%d rows)", text_font_.rows());
    } else {
      S3D_WARN("FONTDEF.FNT not found; the warning banner is disabled");
    }
    const std::string sprites = path_join(asset_dir_, "SDGAME2.PVS");
    if (load_sprite2d(sprites, "left", &arrow_left_) &&
        load_sprite2d(sprites, "righ", &arrow_right_)) {
      S3D_INFO("direction arrows loaded (%dx%d)", arrow_left_.width, arrow_left_.height);
    } else {
      S3D_WARN("SDGAME2.PVS arrows not found; the direction arrow is disabled");
    }
    // The crash explosion animation, cycled one shape per 4 frames while the car
    // is crashed (frame.c shape_op_explosion).
    const char* ex_names[3] = {"ex01", "ex02", "ex03"};
    int ex_loaded = 0;
    for (int i = 0; i < 3; ++i) {
      if (load_sprite2d(sprites, ex_names[i], &explosion_[i])) ++ex_loaded;
    }
    if (ex_loaded == 3) {
      S3D_INFO("crash explosion sprites loaded (%dx%d)", explosion_[0].width,
               explosion_[0].height);
    } else {
      S3D_WARN("SDGAME2.PVS explosion sprites not found; the crash explosion is disabled");
    }
    // The crashed-windscreen pattern (seg003.asm init_crak) lives in GAME.PRE.
    if (crash_.load_from_game_pre(path_join(asset_dir_, "GAME.PRE"))) {
      S3D_INFO("crash overlay loaded: %zu segments, %d stages", crash_.segments().size(),
               crash_.stage_count());
    } else {
      S3D_WARN("GAME.PRE crak/cinf not found; the crash overlay is disabled");
    }
  }
  if (!hud_.init()) S3D_WARN("HUD renderer initialization failed");
  hud_fg_ = have_palette ? palette_color(palette_, 15) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  hud_shadow_ = have_palette ? palette_color(palette_, 0) : glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

  // ---- Horizon scenery -----------------------------------------------------
  // load_skybox (seg003.asm): the track's theme archive holds the four scenery
  // layers of the 1024-pixel panorama. The sky/ground fills use two fixed
  // material_color_list entries.
  sprites_.init();
  if (have_palette && !asset_dir_.empty()) {
    const char* theme = track_.skybox_name();
    if (world::load_skybox(asset_dir_, theme, &skybox_)) {
      for (int i = 0; i < world::kSkyboxLayerCount; ++i) {
        const Sprite2D& s = skybox_.layers[static_cast<size_t>(i)].sprite;
        std::vector<uint8_t> rgba(s.pixels.size() * 4);
        for (size_t p = 0; p < s.pixels.size(); ++p) {
          const uint8_t* c = palette_.rgb8[s.pixels[p]];
          rgba[p * 4 + 0] = c[0];
          rgba[p * 4 + 1] = c[1];
          rgba[p * 4 + 2] = c[2];
          rgba[p * 4 + 3] = 255;
        }
        skybox_textures_[i].upload_rgba(s.width, s.height, rgba.data());
      }
      sky_color_ = world::material_color(world::kMaterialSkyboxSky, palette_);
      ground_color_ = world::material_color(world::kMaterialSkyboxGround, palette_);
      S3D_INFO("skybox: %s loaded (%d layers, sky rgb %.2f/%.2f/%.2f)", theme,
               world::kSkyboxLayerCount, sky_color_.r, sky_color_.g, sky_color_.b);
    } else {
      S3D_WARN("skybox: %s.PVS not found; the horizon scenery is disabled", theme);
    }
  } else {
    S3D_WARN("no palette; the horizon scenery is disabled");
  }

  // ---- Cockpit dashboard ---------------------------------------------------
  // The car's own dashboard archive (STDA<ID>.PVS). Its "dash" panel and "roof"
  // frame are drawn opaquely over the bands outside the windshield; the
  // original shows this by default (dashb_toggle = 1 in a fresh race).
  if (have_palette && !asset_dir_.empty() &&
      world::load_dashboard(asset_dir_, kPlayerCarId, &dashboard_)) {
    const Sprite2D* shapes[2] = {&dashboard_.dash, &dashboard_.roof};
    for (int i = 0; i < 2; ++i) {
      const Sprite2D& s = *shapes[i];
      std::vector<uint8_t> rgba(s.pixels.size() * 4);
      for (size_t p = 0; p < s.pixels.size(); ++p) {
        const uint8_t* c = palette_.rgb8[s.pixels[p]];
        rgba[p * 4 + 0] = c[0];
        rgba[p * 4 + 1] = c[1];
        rgba[p * 4 + 2] = c[2];
        rgba[p * 4 + 3] = 255;
      }
      dashboard_textures_[i].upload_rgba(s.width, s.height, rgba.data());
    }
  } else if (have_palette) {
    S3D_WARN("dashboard: STDACOUN.PVS not found; the cockpit panel is disabled");
  }

  reset_car();
  S3D_INFO("drive scene ready");
  return true;
}

void DriveScene::build_car_meshes() {
  if (car_lib_ == nullptr) return;

  // Bodywork is shaded with the original material table: every non-rear-light
  // paint byte is a material index resolved through material_color_list, just
  // like the track and scenery. The shape's livery slot is
  // gameconfig.game_playermaterial (default 0).
  //
  // Rear-light colour: the original swaps paint job 0x2D for
  // `backlights_paint_override`, material 0x2E when coasting and 0x2F while
  // braking (frame.c). Those materials resolve to palette 4 / 12.
  const glm::vec4 backlight_off = have_palette_ ? world::material_color(0x2E, palette_)
                                                : glm::vec4(0.67f, 0.0f, 0.0f, 1.0f);
  const glm::vec4 backlight_on =
      have_palette_ ? world::material_color(0x2F, palette_) : glm::vec4(1.0f, 0.33f, 0.33f, 1.0f);

  // Resolves one car primitive paint byte. Without SDMAIN.PVS the material table
  // has no palette to sample, so fall back to the placeholder track colours
  // (still routing the rear-light sentinel to `backlight`).
  const Palette* pal = have_palette_ ? &palette_ : nullptr;
  const auto car_color = [pal](const glm::vec4& backlight) -> world::PaintColorFn {
    return [pal, backlight](uint8_t paint) -> glm::vec4 {
      if (pal != nullptr) return world::car_primitive_color(paint, *pal, backlight);
      return paint == world::kCarBacklightPaint ? backlight : world::track_paint_color(paint);
    };
  };

  // frame.c calls sub_204AE on the car shape every frame with the steering angle
  // and car_rc2, so the front wheels steer and every wheel rides its suspension.
  MeshBuilder mb;
  world::append_car_model(mb, *car_lib_, glm::mat4(1.0f), car_color(backlight_off), 0,
                          &wheel_pose_);
  // The player car local bounding box (used to project the car to screen for the
  // crash explosion) is captured at the rest pose so it stays stable.
  if (!car_bounds_valid_) {
    const std::vector<Vertex>& verts = mb.vertices();
    if (!verts.empty()) {
      car_bounds_min_ = car_bounds_max_ = verts.front().position;
      for (const Vertex& v : verts) {
        car_bounds_min_ = glm::min(car_bounds_min_, v.position);
        car_bounds_max_ = glm::max(car_bounds_max_, v.position);
      }
      car_bounds_valid_ = true;
    }
  }
  car_mesh_ = mb.to_mesh();

  MeshBuilder mbb;
  world::append_car_model(mbb, *car_lib_, glm::mat4(1.0f), car_color(backlight_on), 0,
                          &wheel_pose_);
  car_mesh_braking_ = mbb.to_mesh();
}

void DriveScene::reset_car() {
  car_ = physics::CarState{};
  crash_end_frame_ = -1;
  car_.car_transmission = physics::kTransmissionAutomatic;
  car_.car_current_gear = 1;
  car_.car_changing_gear = physics::kGearChangeInactive;
  car_.car_gearratio = spec_.gear_ratios[1];
  car_.car_gearratioshr8 = static_cast<uint16_t>(car_.car_gearratio >> 8);
  car_.car_currpm = spec_.idle_rpm;
  car_.car_sumSurfRearWheels = 1;
  car_.car_sumSurfAllWheels = 1;

  // Spawn on the start/finish line, matching init_game_state (restunts.c): the
  // car sits 210 + 36 world units behind the line along the track direction and
  // faces -track_angle. The physics pivot rests kCarBodyDrop above the ground.
  int spawn_x = static_cast<int>(world::cell_center_x(7));
  int spawn_z = static_cast<int>(world::cell_center_z(5));
  car_.car_rotate.x = 0;  // yaw
  car_.car_rotate.y = 0;  // pitch
  car_.car_rotate.z = 0;  // roll
  if (start_finish_.found) {
    const int angle = start_finish_.angle;
    const auto sin_fast = [](int a) { return fixed::sin_fast(static_cast<unsigned short>(a)); };
    const auto cos_fast = [](int a) { return fixed::cos_fast(static_cast<unsigned short>(a)); };
    const int tmpcol =
        fixed::multiply_and_scale(sin_fast(angle + 0x200), 210) +
        fixed::multiply_and_scale(sin_fast(angle + 0x100), 36);
    const int tmprow =
        fixed::multiply_and_scale(cos_fast(angle + 0x200), 210) +
        fixed::multiply_and_scale(cos_fast(angle + 0x100), 36);
    spawn_x = static_cast<int>(world::cell_center_x(start_finish_.col)) + tmpcol;
    spawn_z = static_cast<int>(world::cell_center_z(start_finish_.row)) + tmprow;
    car_.car_rotate.x = physics::wrap_negate(static_cast<int16_t>(angle));
  }

  const physics::GroundSample g =
      ground_sample(track_, static_cast<float>(spawn_x), static_cast<float>(spawn_z));
  car_.car_position.x = spawn_x * 64;
  car_.car_position.z = spawn_z * 64;
  car_.car_position.y = (g.height + kCarBodyDrop) * 64;
  car_.car_previous_position = car_.car_position;
  display_prev_ = car_;
  accumulator_ = 0.0;

  // The reference resets the race clock (game_frame) and finish state at race
  // setup; "R" restarts the run the same way.
  game_frame_ = 0;
  race_.reset();

  // Return the wheels to their rest pose (steering centred, suspension relaxed).
  wheel_pose_ = world::CarWheelPose{};
  build_car_meshes();

  // Snap the camera behind the car so the first frame is already framed.
  const glm::vec3 car_pos = car_world_position(0.0f);
  camera_.position = car_pos + glm::vec3(0.0f, kFollowHeight, -kFollowDistance);
  aim_at(camera_, car_pos + glm::vec3(0.0f, kFollowAimHeight, 0.0f));
  camera_prev_ = camera_;
}

void DriveScene::physics_tick(const Controls& controls) {
  ++game_frame_;      // drives the animated track objects (windmill, sign, ...)
  race_.tick();       // 20 Hz race clock (restunts state.game_frame)

  uint8_t input_flags = physics::kInputNone;
  if (controls.throttle > 0.05f) input_flags |= physics::kInputAccelerate;
  if (controls.throttle < -0.05f) input_flags |= physics::kInputBrake;
  if (controls.steer > 0.05f) input_flags |= physics::kInputSteerRight;
  if (controls.steer < -0.05f) input_flags |= physics::kInputSteerLeft;

  // A crashed car ignores the player: the original player_op forces the brake
  // until the car stops (state.c).
  physics::apply_crash_control(car_, &input_flags);

  physics::update_car_speed(input_flags, spec_, &car_, physics::kGameFrameRateNormal);
  physics::update_player_steering(input_flags, &car_, physics::kGameFrameRateNormal);
  physics::update_grip(&car_, spec_);
  const auto sampler = [this](float wx, float wz) { return ground_sample(track_, wx, wz); };
  physics::step_car_motion(&car_, spec_, physics::kGameFrameRateNormal, sampler, &collision_);

  // Animate the wheels exactly like the original: frame.c calls sub_204AE every
  // frame with car_steeringAngle and car_rc2 (the per-wheel suspension travel),
  // so the front wheels steer and every wheel follows its suspension.
  //
  // sub_204AE caches the last steering angle (`word_443E8[4]`, loaded as 0) and
  // only re-applies the x/z transform when it changes. The mesh is rebuilt from
  // the rest shape here, so mirror that cache: the authored mesh stays until the
  // car first steers, and once `posed` is set the transform is re-applied for
  // every angle change -- including back at 0, where it degenerates to the
  // original's point reflection about each front-wheel pivot.
  world::CarWheelPose next_wheel_pose = wheel_pose_;
  if (!next_wheel_pose.posed) {
    if (car_.car_steeringAngle != 0) {
      next_wheel_pose.posed = true;
      next_wheel_pose.steering_angle = car_.car_steeringAngle;
    }
  } else if (car_.car_steeringAngle != next_wheel_pose.steering_angle) {
    next_wheel_pose.steering_angle = car_.car_steeringAngle;
  }
  for (int i = 0; i < 4; ++i) {
    next_wheel_pose.suspension[i] = car_.car_suspension_deflection[i];
  }
  if (next_wheel_pose != wheel_pose_) {
    wheel_pose_ = next_wheel_pose;
    build_car_meshes();
  }

  // game_pEndFrame (statecrs.c loc_19760): the tick the crash started. It is set
  // once, on the frame car_crashBmpFlag transitions from 0 to any crash state,
  // and is the animation origin for the cracked-windscreen (init_crak) and
  // sinking (do_sinking) overlays.
  if (car_.car_crashBmpFlag != physics::kCrashEventNone && crash_end_frame_ < 0) {
    crash_end_frame_ = game_frame_;
  }

  // Finish-line test (state.c loc_17771): the run only finishes once the car
  // has returned to route node 0 after passing another node (playerstate
  // field_CD). On the forward crossing the original sets car_crashBmpFlag = 3
  // and records the total finish time.
  if (race_.update_finish(car_.car_position.x, car_.car_position.z, start_finish_)) {
    car_.car_crashBmpFlag = 3;
    S3D_INFO("finish: %s", game::format_race_time(race_.total_finish, 20, true).c_str());
  }

  // Checkpoint/arrow machine (state.c loc_173B3..loc_17699): tracks the
  // checkpoint points ahead of the car to drive the HUD arrow (field_45D) and
  // to clear the wrong-way latch once the car is heading the right way again.
  game::CarPose pose;
  pose.x = car_.car_position.x;
  pose.y = car_.car_position.y;
  pose.z = car_.car_position.z;
  pose.rot_x = car_.car_rotate.x;  // yaw
  pose.rot_y = car_.car_rotate.y;  // pitch
  pose.rot_z = car_.car_rotate.z;  // roll
  pose.crashed = car_.car_crashBmpFlag != 0;
  race_.update_checkpoint(track_, pose);
}

void DriveScene::update_camera() {
  const glm::vec3 car_pos = car_world_position(1.0f);
  const float yaw = car_yaw(1.0f);
  // The car's nose in world space is R_y(-yaw) * +Z = (-sin, 0, cos) with the
  // corrected mat_rot_y (the old transposed matrix mirrored this on X). The
  // camera rides behind that nose.
  const glm::vec3 fwd(-std::sin(yaw), 0.0f, std::cos(yaw));
  const glm::vec3 ideal = car_pos - fwd * kFollowDistance + glm::vec3(0.0f, kFollowHeight, 0.0f);

  const glm::vec3 delta = ideal - camera_.position;
  float horizontal = std::hypot(delta.x, delta.z);
  const float h_step = std::min(horizontal, kFollowHorizontalStep);
  if (horizontal < 0.0001f) horizontal = 1.0f;
  camera_.position += glm::vec3(delta.x / horizontal * h_step,
                                std::clamp(delta.y, -kFollowVerticalStep, kFollowVerticalStep),
                                delta.z / horizontal * h_step);
  aim_at(camera_, car_pos + glm::vec3(0.0f, kFollowAimHeight, 0.0f));
}

void DriveScene::update(double dt, const Controls& controls) {
  if (controls.reset_car) reset_car();
  // handle_ingame_kb_shortcuts: 'D' toggles the cockpit dashboard.
  if (controls.toggle_dashboard && dashboard_.valid()) {
    dashboard_on_ = !dashboard_on_;
  }

  accumulator_ += dt;
  int steps = 0;
  while (accumulator_ >= kPhysicsDt && steps < kMaxPhysicsStepsPerFrame) {
    accumulator_ -= kPhysicsDt;
    ++steps;
    // Snapshot the poses immediately before the step so the renderer can
    // interpolate *within* the last physics tick (and the camera stays locked
    // to the car). Capturing once per update() instead would drop the sub-step
    // interpolation and make the car/camera advance in 20 Hz jumps.
    display_prev_ = car_;
    camera_prev_ = camera_;
    physics_tick(controls);
    update_camera();
  }
}

float DriveScene::render_alpha() const {
  return std::clamp(static_cast<float>(accumulator_ / kPhysicsDt), 0.0f, 1.0f);
}

glm::vec3 DriveScene::car_world_position(float alpha) const {
  const float scale = 1.0f / 64.0f;
  const auto lerp = [alpha, scale](int32_t prev, int32_t curr) {
    return (static_cast<float>(prev) + (static_cast<float>(curr) - static_cast<float>(prev)) * alpha) *
           scale;
  };
  return glm::vec3(lerp(display_prev_.car_position.x, car_.car_position.x),
                   lerp(display_prev_.car_position.y, car_.car_position.y),
                   lerp(display_prev_.car_position.z, car_.car_position.z));
}

float DriveScene::car_yaw(float alpha) const {
  return lerp_angle(display_prev_.car_rotate.x, car_.car_rotate.x, alpha) * kAngleToRadians;
}

float DriveScene::car_pitch(float alpha) const {
  return lerp_angle(display_prev_.car_rotate.y, car_.car_rotate.y, alpha) * kAngleToRadians;
}

float DriveScene::car_roll(float alpha) const {
  return lerp_angle(display_prev_.car_rotate.z, car_.car_rotate.z, alpha) * kAngleToRadians;
}

glm::mat4 DriveScene::car_model_matrix(float alpha) const {
  // Interpolate the 0x400-unit angles first, then rebuild the physics
  // body-to-world rotation (mat_rot_zxy(-roll, -pitch, -yaw)). The matrix is
  // converted to GLM the same way the track builder's fixed_model does, so the
  // visible car is exactly the physics pose (no sign guessing at the GL layer).
  const int16_t yaw256 = static_cast<int16_t>(lerp_angle(display_prev_.car_rotate.x, car_.car_rotate.x, alpha));
  const int16_t pitch256 = static_cast<int16_t>(lerp_angle(display_prev_.car_rotate.y, car_.car_rotate.y, alpha));
  const int16_t roll256 = static_cast<int16_t>(lerp_angle(display_prev_.car_rotate.z, car_.car_rotate.z, alpha));
  const fixed::Mat3 world_rot =
      fixed::mat_rot_zxy(physics::wrap_negate(roll256), physics::wrap_negate(pitch256),
                         physics::wrap_negate(yaw256), 0);

  glm::mat4 model(1.0f);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      model[col][row] = static_cast<float>(world_rot.at(row, col)) / 16384.0f;
    }
  }
  const glm::vec3 pos = car_world_position(alpha);
  model[3][0] = pos.x;
  model[3][1] = pos.y;
  model[3][2] = pos.z;
  model = model * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -kCarBodyDrop, 0.0f));
  return model;
}

bool DriveScene::car_screen_rect(const Camera& camera, float alpha, int width,
                                 int height, glm::vec4* rect) const {
  if (!car_bounds_valid_ || width <= 0 || height <= 0) return false;
  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  const glm::mat4 mvp =
      camera.projection(aspect) * camera.view() * car_model_matrix(alpha);

  glm::vec2 lo(1e30f), hi(-1e30f);
  for (int i = 0; i < 8; ++i) {
    const glm::vec3 p((i & 1) ? car_bounds_max_.x : car_bounds_min_.x,
                      (i & 2) ? car_bounds_max_.y : car_bounds_min_.y,
                      (i & 4) ? car_bounds_max_.z : car_bounds_min_.z);
    const glm::vec4 clip = mvp * glm::vec4(p, 1.0f);
    if (clip.w <= 1e-3f) return false;  // behind the camera
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    const glm::vec2 s((ndc.x * 0.5f + 0.5f) * static_cast<float>(width),
                      (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(height));
    lo = glm::min(lo, s);
    hi = glm::max(hi, s);
  }

  // Clip to the viewport (rect_intersect in the original clipped the car rect to
  // the windshield/viewport before the explosion picked its centre and size).
  const float left = std::max(lo.x, 0.0f);
  const float top = std::max(lo.y, 0.0f);
  const float right = std::min(hi.x, static_cast<float>(width));
  const float bottom = std::min(hi.y, static_cast<float>(height));
  if (right <= left || bottom <= top) return false;
  *rect = glm::vec4(left, top, right, bottom);
  return true;
}

void DriveScene::build_hud(std::vector<Vertex>& verts, int width, int height,
                           const glm::vec4* car_rect) const {
  const int scale = std::max(1, height / 200);

  // Crash explosion (frame.c shape_op_explosion): a crashed car that is still on
  // screen blinks through ex01/ex02/ex03, one shape per 4 game frames, scaled so
  // its width fills the longer side of the (viewport-clipped) car rectangle. The
  // sprite's pivot pixel lands on the rectangle centre. Drawn first, before the
  // crack overlay and the in-game text (frame.c order).
  if (car_.car_crashBmpFlag == physics::kCrashEventCollision && car_rect != nullptr) {
    const int di = (game_frame_ >> 2) % 3;
    const Sprite2D& ex = explosion_[di];
    if (ex.valid()) {
      const float left = car_rect->x, top = car_rect->y;
      const float right = car_rect->z, bottom = car_rect->w;
      const float idx = std::max(right - left, bottom - top);
      const float cx = (left + right) * 0.5f;
      const float cy = (top + bottom) * 0.5f;
      const float pix = idx / static_cast<float>(ex.width);
      append_sprite(verts, ex, palette_, cx - static_cast<float>(ex.pivot_x) * pix,
                    cy - static_cast<float>(ex.pivot_y) * pix, pix);
    }
  }

  // Crashed-windscreen overlay (seg003.asm init_crak). The original draws it
  // before the in-game text (frame.c init_crak precedes draw_ingame_text). The
  // crack coordinates are authored for a 320x200 view: the y axis scales with
  // the view height / 200 and the pattern is centred horizontally.
  if (car_.car_crashBmpFlag == physics::kCrashEventCollision && crash_.valid() &&
      crash_end_frame_ >= 0) {
    const float s = static_cast<float>(scale);
    const float offx = (static_cast<float>(width) - 320.0f * s) * 0.5f;
    const int framecount = std::max(0, game_frame_ - crash_end_frame_);
    const int count = crash_.visible_segments(framecount, physics::kGameFrameRateNormal);
    const auto& segs = crash_.segments();
    const int n = std::min(count, static_cast<int>(segs.size()));
    for (int i = 0; i < n; ++i) {
      const CrashCrack& c = segs[static_cast<size_t>(i)];
      const float x1 = offx + static_cast<float>(c.x1) * s;
      const float x2 = offx + static_cast<float>(c.x2) * s;
      const float y1 = static_cast<float>(c.y1) * s;
      const float y2 = static_cast<float>(c.y2) * s;
      // Each crack line is a white core with a black edge 1px above and below
      // (preRender_line with colours 0, 0 and dialog_fnt_colour = 15).
      append_segment(verts, x1, y1 - s, x2, y2 - s, s, hud_shadow_);
      append_segment(verts, x1, y1 + s, x2, y2 + s, s, hud_shadow_);
      append_segment(verts, x1, y1, x2, y2, s, hud_fg_);
    }
  }

  // Water-crash sinking overlay (seg003.asm do_sinking, frame.c loc_1162): while
  // car_crashBmpFlag == 2 the view floods from the bottom with the skybox water
  // colour. The fill grows linearly to the full view height over
  // framespersec << 2 game frames (the same slot as the crack overlay above:
  // frame.c picks init_crak for flag 1 and do_sinking for flag 2).
  if (car_.car_crashBmpFlag == physics::kCrashEventWater && crash_end_frame_ >= 0) {
    const int framecount = std::max(0, game_frame_ - crash_end_frame_);
    const int fill = sinking_fill_height(framecount, height, physics::kGameFrameRateNormal);
    if (fill > 0) {
      append_quad(verts, 0.0f, static_cast<float>(height - fill),
                  static_cast<float>(width), static_cast<float>(fill),
                  world::material_color(world::kMaterialSkyboxWater, palette_));
    }
  }

  // The original draws the elapsed time with FONTLED at x=0x8C (320-wide screen)
  // and y=roofbmpheight+2; here it sits at the top centre and scales with the
  // window. intro_draw_text draws a black copy offset by one pixel first.
  if (hud_font_.valid()) {
    // Chase-cam HUD shows MM:SS (format_frame_as_string with the hundredths flag
    // clear); the hundredths form belongs to the finished-time/evaluation screen.
    const std::string time_str = game::format_race_time(race_.game_frame, 20, false);
    const float text_w = static_cast<float>(hud_font_.text_width(time_str)) * scale;
    const float x = (static_cast<float>(width) - text_w) * 0.5f;
    // The original draws the clock at y = roofbmpheight + 2 (frame.c): with the
    // cockpit panel shown the roof strip pushes it down, otherwise it stays at
    // the top of the view.
    const int roof_rows =
        (dashboard_on_ && dashboard_.valid()) ? dashboard_.roof_height() : 0;
    const float y = static_cast<float>(std::max(4, roof_rows + 2)) * scale;
    append_text(verts, hud_font_, time_str, x + scale, y + scale, scale, hud_shadow_);
    append_text(verts, hud_font_, time_str, x, y, scale, hud_fg_);
  }

  const float cx = static_cast<float>(width) * 0.5f;
  const float cy = static_cast<float>(height) * 0.5f;

  // Direction arrow / wrong-way warning (seg003.asm draw_ingame_text loc_1D2DC):
  // field_45D 1 = "left" arrow, 2 = "righ" arrow, 3 = "Wrong Way", 0 = none. The
  // original gates this on cameramode 0, no opponent follow and no crash; only
  // the crash flag exists in this port. field_45E (opponent near) is deferred.
  if (car_.car_crashBmpFlag == 0) {
    const Sprite2D* arrow = nullptr;
    if (race_.field_45d == 1) arrow = &arrow_left_;
    if (race_.field_45d == 2) arrow = &arrow_right_;
    if (arrow != nullptr && arrow->valid()) {
      append_sprite(verts, *arrow, palette_,
                    cx - static_cast<float>(arrow->width) * scale * 0.5f,
                    cy - static_cast<float>(arrow->height) * scale * 0.5f, scale);
    } else if (race_.field_45d == 3 && text_font_.valid()) {
      const std::string msg = "Wrong Way";
      const float w = static_cast<float>(text_font_.text_width(msg)) * scale;
      const float y = cy - static_cast<float>(text_font_.rows()) * scale * 0.5f;
      const float x = cx - w * 0.5f;
      append_text(verts, text_font_, msg, x + scale, y + scale, scale, hud_shadow_);
      append_text(verts, text_font_, msg, x, y, scale, hud_fg_);
    }
  }

  // Penalty banner (seg003.asm loc_1D42C): "pen" plus the last penalty's time,
  // shown while show_penalty_counter counts down. Sits just below centre so it
  // does not cover the arrow.
  if (race_.show_penalty_counter > 0 && text_font_.valid()) {
    const std::string msg =
        "pen" + game::format_race_time(race_.penalty_time, 20, false);
    const float w = static_cast<float>(text_font_.text_width(msg)) * scale;
    const float x = cx - w * 0.5f;
    const float y = cy + static_cast<float>(3 * scale);
    append_text(verts, text_font_, msg, x + scale, y + scale, scale, hud_shadow_);
    append_text(verts, text_font_, msg, x, y, scale, hud_fg_);
  }
}

float DriveScene::horizon_row(const Camera& camera, int width, int height) const {
  if (width <= 0 || height <= 0) return 0.0f;
  const float h = static_cast<float>(height);
  glm::vec3 level(camera.forward().x, 0.0f, camera.forward().z);
  const float len = glm::length(level);
  if (len < 1e-5f) return h * 0.5f;  // looking straight up or down
  level /= len;
  // Project the level direction (w = 0) through the camera; its screen row is
  // where the view ray is horizontal, i.e. the horizon.
  const glm::mat4 vp =
      camera.projection(static_cast<float>(width) / h) * camera.view();
  const glm::vec4 clip = vp * glm::vec4(level, 0.0f);
  if (std::abs(clip.w) < 1e-6f) return h * 0.5f;
  const glm::vec3 ndc = glm::vec3(clip) / clip.w;
  return std::clamp((1.0f - (ndc.y * 0.5f + 0.5f)) * h, 0.0f, h);
}

void DriveScene::draw_sky(const Camera& camera, float alpha, int width, int height) {
  if (!skybox_.valid() || width <= 0 || height <= 0) return;
  const float horizon = horizon_row(camera, width, height);

  // Flat sky above the horizon and ground below. skybox_op_helper2 fills the
  // sky only down to `horizon - min_height`, but the opaque layer blits then
  // cover everything from `horizon - max_height` down to the horizon, so
  // filling the whole band first yields the same pixels.
  std::vector<Vertex> fill;
  append_quad(fill, 0.0f, 0.0f, static_cast<float>(width), horizon, sky_color_);
  if (horizon < static_cast<float>(height)) {
    append_quad(fill, 0.0f, horizon, static_cast<float>(width),
                static_cast<float>(height) - horizon, ground_color_);
  }
  hud_.draw(fill, width, height);

  // The panorama scrolls one pixel per 1/1024-turn of camera yaw (the original
  // feeds car_rot_x_2, which matches the car's yaw). Layers stand on the
  // horizon and are blitted opaquely, so they are drawn after the sky fill.
  const int yaw_units =
      static_cast<int>(std::lround(car_yaw(alpha) / kAngleToRadians));

  world::SkyboxPlacement placements[64];
  const int n = world::skybox_layout(skybox_, width, height, horizon, yaw_units,
                                     placements, 64);
  std::vector<Vertex> layer_verts[world::kSkyboxLayerCount];
  for (int i = 0; i < n; ++i) {
    const world::SkyboxPlacement& p = placements[i];
    append_textured_quad(layer_verts[static_cast<size_t>(p.layer)], p.x, p.y, p.width,
                         p.height);
  }
  for (int i = 0; i < world::kSkyboxLayerCount; ++i) {
    sprites_.draw(skybox_textures_[i], layer_verts[static_cast<size_t>(i)], width,
                  height);
  }
}

void DriveScene::draw_dashboard(int width, int height) {
  if (!dashboard_on_ || !dashboard_.valid()) return;
  world::DashboardRects r;
  if (!world::dashboard_rects(dashboard_, width, height, &r)) return;

  // Drawn last so the opaque panel hides the world and HUD outside the
  // windshield (the original composites the dashboard over the 3D band).
  std::vector<Vertex> verts;
  append_textured_quad(verts, r.dash_x, r.dash_y, r.dash_w, r.dash_h);
  sprites_.draw(dashboard_textures_[0], verts, width, height);

  verts.clear();
  append_textured_quad(verts, r.roof_x, r.roof_y, r.roof_w, r.roof_h);
  sprites_.draw(dashboard_textures_[1], verts, width, height);
}

void DriveScene::render(Renderer& renderer, int width, int height) {
  const float alpha = render_alpha();
  // Interpolate the follow camera alongside the car so the two stay locked.
  // The camera otherwise only steps at 20 Hz while the car is interpolated,
  // which makes the car jitter back and forth within the frame.
  Camera render_cam = camera_;
  render_cam.position = glm::mix(camera_prev_.position, camera_.position, alpha);
  aim_at(render_cam, car_world_position(alpha) + glm::vec3(0.0f, kFollowAimHeight, 0.0f));

  // Sky and horizon panorama first, then the world overwrites them. draw_world
  // re-enables depth testing, so the sky pass drawing with it off is safe.
  draw_sky(render_cam, alpha, width, height);

  renderer.draw_world(track_mesh_, render_cam);

  const int phase = world::track_animation_phase(game_frame_);
  renderer.draw_world(animated_meshes_[phase], render_cam);
  renderer.draw_world_lines(animated_lines_[phase], render_cam);

  if (!car_mesh_.empty()) {
    const Mesh& body =
        (car_.car_is_braking != 0 && !car_mesh_braking_.empty()) ? car_mesh_braking_ : car_mesh_;
    renderer.draw_world(body, render_cam, car_model_matrix(alpha));
  }

  glm::vec4 car_rect;
  const bool has_car_rect =
      car_screen_rect(render_cam, alpha, width, height, &car_rect);

  std::vector<Vertex> hud;
  build_hud(hud, width, height, has_car_rect ? &car_rect : nullptr);
  hud_.draw(hud, width, height);

  // The cockpit panel goes last so it masks the world and HUD outside the
  // windshield (seg005.asm loop_game composites it over the 3D band).
  draw_dashboard(width, height);

  ++frame_index_;
  if (!screenshot_path_.empty() && frame_index_ >= screenshot_frame_) {
    capture_frame_png(screenshot_path_, width, height);
    screenshot_path_.clear();
    finished_ = true;
  }
}

void DriveScene::request_screenshot(std::string path, int frames) {
  screenshot_path_ = std::move(path);
  screenshot_frame_ = frames > 0 ? frames : 1;
}

}  // namespace s3d