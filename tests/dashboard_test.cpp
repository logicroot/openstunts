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

TEST(dashboard_steer_value_shares_wheel_selection) {
  // seg005.asm loc_2319D / loc_23561: sign(a) * (|a| >> 3), truncating to zero.
  EXPECT_EQ(world::dashboard_steer_value(0), 0);
  EXPECT_EQ(world::dashboard_steer_value(7), 0);
  EXPECT_EQ(world::dashboard_steer_value(8), 1);
  EXPECT_EQ(world::dashboard_steer_value(-8), -1);
  EXPECT_EQ(world::dashboard_steer_value(80), 10);
  EXPECT_EQ(world::dashboard_steer_value(81), 10);
  EXPECT_EQ(world::dashboard_steer_value(88), 11);
  EXPECT_EQ(world::dashboard_steer_value(-88), -11);
  EXPECT_EQ(world::dashboard_steer_value(-17), -2);
  EXPECT_EQ(world::dashboard_steer_value(1024), 128);
  for (int a = -4096; a <= 4096; a += 37) {
    EXPECT_EQ(world::dashboard_steer_value(static_cast<int16_t>(a)),
              (a < 0 ? -((-a) >> 3) : (a >> 3)));
  }
}

TEST(dashboard_speed_index_matches_asm) {
  // loc_23299: car_speed (2^8 * mph) / 640, clamped to num_points - 1.
  EXPECT_EQ(world::dashboard_speed_index(0, 84), 0);
  EXPECT_EQ(world::dashboard_speed_index(639, 84), 0);
  EXPECT_EQ(world::dashboard_speed_index(640, 84), 1);
  EXPECT_EQ(world::dashboard_speed_index(640 * 83, 84), 83);
  EXPECT_EQ(world::dashboard_speed_index(640 * 84, 84), 83);  // clamped
  EXPECT_EQ(world::dashboard_speed_index(65535, 84), 83);     // clamped
  EXPECT_EQ(world::dashboard_speed_index(640, 1), 0);
  // An empty list has no needle.
  EXPECT_EQ(world::dashboard_speed_index(640, 0), -1);
  EXPECT_EQ(world::dashboard_speed_index(640, -1), -1);
}

TEST(dashboard_rev_index_matches_asm) {
  // loc_232BC: (unsigned 16-bit) rpm >> 7, clamped to num_points - 1.
  EXPECT_EQ(world::dashboard_rev_index(0, 71), 0);
  EXPECT_EQ(world::dashboard_rev_index(127, 71), 0);
  EXPECT_EQ(world::dashboard_rev_index(128, 71), 1);
  EXPECT_EQ(world::dashboard_rev_index(128 * 70, 71), 70);
  EXPECT_EQ(world::dashboard_rev_index(128 * 71, 71), 70);  // clamped
  // The shift is logical on the 16-bit field, so -1 reads as 511 -> clamped.
  EXPECT_EQ(world::dashboard_rev_index(static_cast<int16_t>(-1), 71), 70);
  EXPECT_EQ(world::dashboard_rev_index(0, 0), -1);
}

TEST(dashboard_steer_dot_index_clamps) {
  // loc_235C5: |steering value|, clamped to the list (31 entries for the COUN).
  EXPECT_EQ(world::dashboard_steer_dot_index(0, 31), 0);
  EXPECT_EQ(world::dashboard_steer_dot_index(88, 31), 11);
  EXPECT_EQ(world::dashboard_steer_dot_index(-88, 31), 11);
  EXPECT_EQ(world::dashboard_steer_dot_index(1024, 31), 30);  // 128 -> clamped
  EXPECT_EQ(world::dashboard_steer_dot_index(-248, 31), 30);  // 31 -> clamped
  EXPECT_EQ(world::dashboard_steer_dot_index(88, 0), -1);
}

TEST(dashboard_speedo_kind_from_center_y) {
  // loc_2328A: 0xFFFF -> no needle, 0 -> digital, anything else -> analog.
  EXPECT_TRUE(world::dashboard_speedo_kind(19) == world::SpeedoKind::Analog);
  EXPECT_TRUE(world::dashboard_speedo_kind(-19) == world::SpeedoKind::Analog);
  EXPECT_TRUE(world::dashboard_speedo_kind(0) == world::SpeedoKind::Digital);
  EXPECT_TRUE(world::dashboard_speedo_kind(-1) == world::SpeedoKind::None);
}

TEST(dashboard_point_at_reads_byte_pairs) {
  const uint8_t points[] = {0x10, 0x20, 0x30, 0x40};
  int x = 99, y = 99;
  world::dashboard_point_at(points, 0, &x, &y);
  EXPECT_EQ(x, 0x10);
  EXPECT_EQ(y, 0x20);
  world::dashboard_point_at(points, 1, &x, &y);
  EXPECT_EQ(x, 0x30);
  EXPECT_EQ(y, 0x40);
  world::dashboard_point_at(points, -1, &x, &y);
  EXPECT_EQ(x, 0);
  EXPECT_EQ(y, 0);
  world::dashboard_point_at(nullptr, 0, &x, &y);
  EXPECT_EQ(x, 0);
  EXPECT_EQ(y, 0);
  world::dashboard_point_at(points, 0, nullptr, nullptr);  // must not crash
}

TEST(dashboard_rects_instruments_and_gear) {
  world::Dashboard d = synthetic_dashboard();
  d.ins2.width = 136;
  d.ins2.height = 39;
  d.ins2.pos_x = 64;
  d.ins2.pos_y = 148;
  d.ins2.pixels.assign(136 * 39, 0);
  d.gear_box.width = 64;
  d.gear_box.height = 56;
  d.gear_box.pos_x = 256;
  d.gear_box.pos_y = 144;
  d.gear_box.pixels.assign(64 * 56, 0);
  d.gear_knob.width = 24;
  d.gear_knob.height = 23;
  d.gear_knob.pivot_x = 12;
  d.gear_knob.pivot_y = 11;
  d.gear_knob.pixels.assign(24 * 23, 0);
  d.steer_dot.width = 8;
  d.steer_dot.height = 4;
  d.steer_dot.pivot_x = 5;
  d.steer_dot.pivot_y = 2;
  d.steer_dot.pixels.assign(8 * 4, 0);

  world::DashboardRects r;
  EXPECT_TRUE(world::dashboard_rects(d, 320, 200, &r));
  EXPECT_TRUE(r.has_instruments);
  EXPECT_EQ(r.instrument_x, 64.0f);
  EXPECT_EQ(r.instrument_y, 148.0f);
  EXPECT_EQ(r.instrument_w, 136.0f);
  EXPECT_EQ(r.instrument_h, 39.0f);
  EXPECT_TRUE(r.has_gear_box);
  EXPECT_EQ(r.gear_box_x, 256.0f);
  EXPECT_EQ(r.gear_box_y, 144.0f);
  EXPECT_EQ(r.gear_box_w, 64.0f);
  EXPECT_EQ(r.gear_box_h, 56.0f);
  EXPECT_TRUE(r.has_knob);
  EXPECT_EQ(r.knob_w, 24.0f);
  EXPECT_EQ(r.knob_h, 23.0f);
  EXPECT_EQ(r.knob_pivot_x, 12.0f);
  EXPECT_EQ(r.knob_pivot_y, 11.0f);
  EXPECT_TRUE(r.has_dot);
  EXPECT_EQ(r.dot_w, 8.0f);
  EXPECT_EQ(r.dot_h, 4.0f);
  EXPECT_EQ(r.dot_pivot_x, 5.0f);
  EXPECT_EQ(r.dot_pivot_y, 2.0f);
  EXPECT_EQ(r.scale_x, 1.0f);
  EXPECT_EQ(r.scale_y, 1.0f);

  // At 640x400 the rects scale by 2 in both axes; the pivots stay authored.
  EXPECT_TRUE(world::dashboard_rects(d, 640, 400, &r));
  EXPECT_EQ(r.instrument_x, 128.0f);
  EXPECT_EQ(r.instrument_y, 296.0f);
  EXPECT_EQ(r.instrument_w, 272.0f);
  EXPECT_EQ(r.gear_box_x, 512.0f);
  EXPECT_EQ(r.gear_box_y, 288.0f);
  EXPECT_EQ(r.scale_x, 2.0f);
  EXPECT_EQ(r.scale_y, 2.0f);
  EXPECT_EQ(r.knob_pivot_x, 12.0f);
  EXPECT_EQ(r.dot_pivot_y, 2.0f);

  // Without the optional sprites the flags are false and the rects are zero.
  const world::Dashboard plain = synthetic_dashboard();
  EXPECT_TRUE(world::dashboard_rects(plain, 320, 200, &r));
  EXPECT_TRUE(!r.has_instruments);
  EXPECT_TRUE(!r.has_gear_box);
  EXPECT_TRUE(!r.has_knob);
  EXPECT_TRUE(!r.has_dot);
  EXPECT_EQ(r.instrument_x, 0.0f);
  EXPECT_EQ(r.gear_box_x, 0.0f);
}

TEST(dashboard_load_player_car_instruments) {
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
  // ins2 (instrument face) lives in STDA<car>.PVS at the needle anchor.
  EXPECT_TRUE(d.instruments_loaded);
  EXPECT_EQ(d.ins2.width, 136);
  EXPECT_EQ(d.ins2.height, 39);
  EXPECT_EQ(d.ins2.pos_x, 64);
  EXPECT_EQ(d.ins2.pos_y, 148);
  // gbox (gear-selector plate) anchors the knob offsets.
  EXPECT_TRUE(d.gear_box.valid());
  EXPECT_EQ(d.gear_box.width, 64);
  EXPECT_EQ(d.gear_box.height, 56);
  EXPECT_EQ(d.gear_box.pos_x, 256);
  EXPECT_EQ(d.gear_box.pos_y, 144);
  // gnob/dot are pivot-anchored sprites from STDB<car>.PVS.
  EXPECT_TRUE(d.gear_knob.valid());
  EXPECT_EQ(d.gear_knob.width, 24);
  EXPECT_EQ(d.gear_knob.height, 23);
  EXPECT_EQ(d.gear_knob.pivot_x, 12);
  EXPECT_EQ(d.gear_knob.pivot_y, 11);
  EXPECT_TRUE(d.steer_dot.valid());
  EXPECT_EQ(d.steer_dot.width, 8);
  EXPECT_EQ(d.steer_dot.height, 4);
  EXPECT_EQ(d.steer_dot.pivot_x, 5);
  EXPECT_EQ(d.steer_dot.pivot_y, 2);
}
