// Cockpit dashboard tests. Covers the pure screen-space layout maths (no
// assets) and, when the Stunts asset directory is present, loading the player
// car's "dash"/"roof" panels from STDACOUN.PVS (seg005.asm setup_car_shapes).

#include <string>

#include "assets/sprite2d.h"
#include "core/paths.h"
#include "tiny_test.h"
#include "world/dashboard.h"

using namespace s3d;

namespace {

world::Dashboard synthetic_dashboard() {
  world::Dashboard d;
  d.roof.width = 320;
  d.roof.height = 9;
  d.roof.pos_x = 0;
  d.roof.pos_y = 0;
  d.roof.pixels.assign(320 * 9, 0);
  d.dash.width = 320;
  d.dash.height = 70;
  d.dash.pos_x = 0;
  d.dash.pos_y = 130;
  d.dash.pixels.assign(320 * 70, 0);
  d.loaded = true;
  return d;
}

}  // namespace

TEST(dashboard_leader_rects_authored) {
  const world::Dashboard d = synthetic_dashboard();
  world::DashboardRects r;
  EXPECT_TRUE(world::dashboard_rects(d, 320, 200, &r));
  // At the authored 320x200 view the rectangles are the SHAPE2D positions.
  EXPECT_EQ(r.roof_x, 0.0f);
  EXPECT_EQ(r.roof_y, 0.0f);
  EXPECT_EQ(r.roof_w, 320.0f);
  EXPECT_EQ(r.roof_h, 9.0f);
  EXPECT_EQ(r.dash_x, 0.0f);
  EXPECT_EQ(r.dash_y, 130.0f);
  EXPECT_EQ(r.dash_w, 320.0f);
  EXPECT_EQ(r.dash_h, 70.0f);
  EXPECT_EQ(r.windshield_top, 9.0f);
  EXPECT_EQ(r.windshield_bottom, 130.0f);
}

TEST(dashboard_rects_scale_to_view) {
  const world::Dashboard d = synthetic_dashboard();
  world::DashboardRects r;
  // x scales with the view width, y with the view height.
  EXPECT_TRUE(world::dashboard_rects(d, 640, 400, &r));
  EXPECT_EQ(r.roof_w, 640.0f);
  EXPECT_EQ(r.roof_h, 18.0f);
  EXPECT_EQ(r.dash_y, 260.0f);
  EXPECT_EQ(r.dash_h, 140.0f);
  EXPECT_EQ(r.windshield_top, 18.0f);
  EXPECT_EQ(r.windshield_bottom, 260.0f);

  EXPECT_TRUE(world::dashboard_rects(d, 1280, 720, &r));
  EXPECT_EQ(r.roof_w, 1280.0f);
  EXPECT_EQ(r.roof_h, 9.0f * 3.6f);
  EXPECT_EQ(r.dash_y, 130.0f * 3.6f);
  EXPECT_EQ(r.dash_h, 70.0f * 3.6f);
  EXPECT_EQ(r.windshield_bottom, 130.0f * 3.6f);
}

TEST(dashboard_rects_spans_full_width) {
  const world::Dashboard d = synthetic_dashboard();
  // The panel is authored full-width, so it must always reach both edges.
  for (int w : {320, 640, 1024, 1280, 1920}) {
    world::DashboardRects r;
    EXPECT_TRUE(world::dashboard_rects(d, w, 200, &r));
    EXPECT_EQ(r.dash_x, 0.0f);
    EXPECT_EQ(r.dash_w, static_cast<float>(w));
    EXPECT_EQ(r.roof_w, static_cast<float>(w));
  }
}

TEST(dashboard_rects_wheel) {
  world::Dashboard d = synthetic_dashboard();
  for (int i = 0; i < 3; ++i) {
    d.wheel[i].width = 184;
    d.wheel[i].height = 32;
    d.wheel[i].pos_x = 40;
    d.wheel[i].pos_y = 168;
    d.wheel[i].pixels.assign(184 * 32, 0);
  }
  world::DashboardRects r;
  EXPECT_TRUE(world::dashboard_rects(d, 320, 200, &r));
  EXPECT_TRUE(r.has_wheel);
  EXPECT_EQ(r.wheel_x, 40.0f);
  EXPECT_EQ(r.wheel_y, 168.0f);
  EXPECT_EQ(r.wheel_w, 184.0f);
  EXPECT_EQ(r.wheel_h, 32.0f);

  EXPECT_TRUE(world::dashboard_rects(d, 640, 400, &r));
  EXPECT_EQ(r.wheel_x, 80.0f);
  EXPECT_EQ(r.wheel_y, 336.0f);
  EXPECT_EQ(r.wheel_w, 368.0f);
  EXPECT_EQ(r.wheel_h, 64.0f);
}

TEST(dashboard_rects_without_wheel) {
  const world::Dashboard d = synthetic_dashboard();
  world::DashboardRects r;
  EXPECT_TRUE(world::dashboard_rects(d, 320, 200, &r));
  EXPECT_TRUE(!r.has_wheel);
  EXPECT_EQ(r.wheel_w, 0.0f);
  EXPECT_EQ(r.wheel_h, 0.0f);
}

TEST(dashboard_rects_rejects_invalid) {
  world::Dashboard unloaded;
  world::DashboardRects r;
  EXPECT_TRUE(!world::dashboard_rects(unloaded, 320, 200, &r));
  EXPECT_TRUE(!world::dashboard_rects(unloaded, 320, 200, nullptr));

  const world::Dashboard d = synthetic_dashboard();
  EXPECT_TRUE(!world::dashboard_rects(d, 0, 200, &r));
  EXPECT_TRUE(!world::dashboard_rects(d, 320, 0, &r));
  EXPECT_TRUE(!world::dashboard_rects(d, 320, 200, nullptr));
}

TEST(dashboard_fallbacks_without_assets) {
  const world::Dashboard unloaded;
  EXPECT_EQ(unloaded.roof_height(), 0);
  EXPECT_EQ(unloaded.dash_top(), 200);  // 0xC8 full-height fallback
  EXPECT_TRUE(!unloaded.valid());
}

TEST(dashboard_load_missing_car) {
  world::Dashboard d;
  EXPECT_TRUE(!world::load_dashboard("", "COUN", &d));
  EXPECT_TRUE(!d.valid());
  EXPECT_TRUE(!world::load_dashboard("does/not/exist", "COUN", &d));
  EXPECT_TRUE(!world::load_dashboard("/home/firecooler/stunts", "ZZZZ", &d));
  EXPECT_TRUE(!d.valid());
}

TEST(dashboard_load_player_car) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  world::Dashboard d;
  if (!world::load_dashboard(dir, "COUN", &d)) {
    std::printf("  (skipped: STDACOUN.PVS not found)\n");
    return;
  }
  EXPECT_TRUE(d.valid());
  // The Countach panel: a 320x70 dash whose pos_y is dashbmp_y, and a 320x9 roof.
  EXPECT_EQ(d.dash.width, 320);
  EXPECT_EQ(d.dash.height, 70);
  EXPECT_EQ(d.dash.pos_y, 130);
  EXPECT_EQ(d.roof.width, 320);
  EXPECT_EQ(d.roof.height, 9);
  EXPECT_EQ(d.dash_top(), 130);
  EXPECT_EQ(d.roof_height(), 9);
  // The three steering-wheel sprites, all 184x32 at (40,168).
  EXPECT_TRUE(d.wheels_loaded);
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(d.wheel[i].valid());
    EXPECT_EQ(d.wheel[i].width, 184);
    EXPECT_EQ(d.wheel[i].height, 32);
    EXPECT_EQ(d.wheel[i].pos_x, 40);
    EXPECT_EQ(d.wheel[i].pos_y, 168);
  }
  EXPECT_TRUE(d.any_wheel() != nullptr);
}

TEST(dashboard_wheel_index_dead_zone) {
  // seg005.asm loc_2319D: trunc(angle/8) with a +-10 dead zone -> 0/1/2.
  EXPECT_EQ(world::dashboard_wheel_index(0), 1);
  EXPECT_EQ(world::dashboard_wheel_index(8), 1);
  EXPECT_EQ(world::dashboard_wheel_index(-8), 1);
  EXPECT_EQ(world::dashboard_wheel_index(80), 1);   // 10 -> centred
  EXPECT_EQ(world::dashboard_wheel_index(-80), 1);
  EXPECT_EQ(world::dashboard_wheel_index(88), 2);   // 11 -> turned right
  EXPECT_EQ(world::dashboard_wheel_index(-88), 0);  // -11 -> turned left
  EXPECT_EQ(world::dashboard_wheel_index(1024), 2);
  EXPECT_EQ(world::dashboard_wheel_index(-1024), 0);
}

TEST(dashboard_wheel_index_truncates_toward_zero) {
  // The original negates (|angle| >> 3) instead of arithmetic-shifting, so it
  // truncates toward zero: -17 -> -2 (centred), not -3.
  EXPECT_EQ(world::dashboard_wheel_index(-17), 1);
  EXPECT_EQ(world::dashboard_wheel_index(-79), 1);  // 9 -> centred
  EXPECT_EQ(world::dashboard_wheel_index(-81), 1);  // 10 -> centred
  EXPECT_EQ(world::dashboard_wheel_index(-88), 0);  // 11 -> left
}
