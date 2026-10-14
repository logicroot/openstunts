// Track collision tests: GAME.PRE plan/wall loading, the two math.c helpers
// (plane_signed_distance / transform_wheel_travel_to_world), and the
// build_track_object element dispatch (seg004.asm) on synthetic tracks.
//
// Tests that need the shipped plan/wall tables or a real .TRK skip themselves
// when no Stunts asset directory is found, matching the rest of the suite.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "assets/trk.h"
#include "core/paths.h"
#include "physics/fixed_math.h"
#include "physics/track_collision.h"
#include "tiny_test.h"
#include "world/track_objects.h"

using namespace s3d;
using namespace s3d::physics;

namespace {

bool load_collision(TrackCollision* tc) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) return false;
  return tc->load_from_game_pre(path_join(dir, "GAME.PRE"));
}

Track empty_track() {
  Track t;
  t.sky = 0;
  return t;
}

// Stores an element byte at cell (x, y) (index = y*30 + x in this project).
void set_item(Track* t, int x, int y, uint8_t item) {
  t->items[static_cast<size_t>(y) * Track::kSize + x] = item;
}

void set_terrain(Track* t, int x, int y, uint8_t terrain) {
  t->terrain[static_cast<size_t>(y) * Track::kSize + x] = terrain;
}

fixed::Vec3 word_pos(int x, int y, int z) {
  fixed::Vec3 v;
  v.x = static_cast<int16_t>(x);
  v.y = static_cast<int16_t>(y);
  v.z = static_cast<int16_t>(z);
  return v;
}

}  // namespace

// ---------------------------------------------------------------------------
// Loader
// ---------------------------------------------------------------------------
TEST(track_collision_loads_plan_and_wall_tables) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  EXPECT_TRUE(tc.is_loaded());
  EXPECT_EQ(tc.plane_count(), 536u);
  EXPECT_EQ(tc.wall_count(), 191u);

  const Plane* p = tc.planes();
  // Plane 0: flat ground at y = 0 with an upward normal of length 0x2000.
  EXPECT_EQ(p[0].plane_origin.y, 0);
  EXPECT_EQ(p[0].plane_normal.x, 0);
  EXPECT_EQ(p[0].plane_normal.y, 8192);
  EXPECT_EQ(p[0].plane_normal.z, 0);
  // Plane 4 (highway) is the flat plane raised to y = 24.
  EXPECT_EQ(p[4].plane_origin.y, 24);
  // Plane 8 (elevated/solid road) sits at y = 450.
  EXPECT_EQ(p[8].plane_origin.y, 450);
}

// ---------------------------------------------------------------------------
// plane_signed_distance (math.c:826)
// ---------------------------------------------------------------------------
TEST(track_collision_plane_signed_distance_flat_planes) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  // Plane < 4: distance is y minus the accumulated terrain height.
  EXPECT_EQ(tc.plane_signed_distance(0, 100, 50, 200, 0, 0, 0), 50);
  EXPECT_EQ(tc.plane_signed_distance(0, 100, 50, 200, 7, 123, 456), 43);
  // No plane (-1) falls back to the same flat rule.
  EXPECT_EQ(tc.plane_signed_distance(-1, 100, 50, 200, 7, 0, 0), 43);
  // Plane 4 (highway, origin y = 24) subtracts the 24 elevation.
  EXPECT_EQ(tc.plane_signed_distance(4, 100, 100, 200, 0, 0, 0), 76);
  // Plane 8 (elevated, origin y = 450).
  EXPECT_EQ(tc.plane_signed_distance(8, 100, 500, 200, 0, 0, 0), 50);
}

TEST(track_collision_plane_signed_distance_tilted_plane) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  // Plane 12 has origin (120, 0, -512) and normal (0, 7499, -3295). Evaluating
  // at the origin+element-centre leaves only the y term: 7499 * 8192 / 8192.
  EXPECT_EQ(tc.plane_signed_distance(12, 120, 8192, -512, 0, 0, 0), 7499);
  // For plane_index >= 4 the element centre offsets the origin; for < 4 it is
  // ignored entirely.
  EXPECT_EQ(tc.plane_signed_distance(0, 100, 50, 200, 0, 123, 456),
            tc.plane_signed_distance(0, 100, 50, 200, 0, 0, 0));
  EXPECT_TRUE(tc.plane_signed_distance(12, 120, 8192, -512, 0, 0, 100) !=
              tc.plane_signed_distance(12, 120, 8192, -512, 0, 0, 0));
}

// ---------------------------------------------------------------------------
// transform_wheel_travel_to_world (math.c:864 plane_rotate_op)
// ---------------------------------------------------------------------------
TEST(track_collision_transform_no_plane_matches_car_matrix) {
  // The no-plane branch never touches the plane tables, so no assets needed.
  TrackCollision tc;
  const fixed::Mat3 identity;
  fixed::Vec3 out;

  tc.transform_wheel_travel_to_world(100, 0, identity, 0, 0, 0, -1, &out);
  EXPECT_EQ(out.x, 0);
  EXPECT_EQ(out.y, 0);
  EXPECT_EQ(out.z, 100);  // car_to_world * (0, 0, travel)

  // A quarter-turn wheel heading rotates the travel into -X (mat_rot_y(-90)).
  tc.transform_wheel_travel_to_world(100, 0x100, identity, 0, 0, 0, -1, &out);
  EXPECT_EQ(out.y, 0);
  EXPECT_TRUE(std::abs(out.x) >= 99 && std::abs(out.x) <= 100);
  EXPECT_TRUE(std::abs(out.z) <= 1);
}

TEST(track_collision_transform_flat_plane_keeps_travel) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  const fixed::Mat3 identity;
  fixed::Vec3 out;
  // Plane 0 is the identity orientation and matches pitch/roll == 0, so with a
  // zero wheel heading the travel passes through unchanged.
  tc.transform_wheel_travel_to_world(100, 0, identity, 0, 0, 0, 0, &out);
  EXPECT_EQ(out.x, 0);
  EXPECT_EQ(out.y, 0);
  EXPECT_EQ(out.z, 100);
}

// ---------------------------------------------------------------------------
// build_track_object (seg004.asm)
// ---------------------------------------------------------------------------
TEST(track_collision_build_object_needs_bound_track) {
  // Without a bound track the result is the flat grass default.
  TrackCollision tc;
  CollisionSample s;
  tc.build_track_object(word_pos(3000, 0, 4000), word_pos(3000, 0, 4000), &s);
  EXPECT_EQ(s.plane_index, 0);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Grass));
  EXPECT_EQ(s.wall_index, -1);

  // The explicit overload is also flat when the plan/wall tables are absent.
  Track t = empty_track();
  tc.build_track_object_from_track(t, word_pos(3000, 0, 4000), word_pos(3000, 0, 4000), &s);
  EXPECT_EQ(s.plane_index, 0);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Grass));
}

TEST(track_collision_build_object_flat_grass) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  tc.set_track(&t);

  // Cell (3, 4) centre: no element, no special terrain -> flat grass.
  const int wx = 3 * 1024 + 512;
  const int wz = 4 * 1024 + 512;
  CollisionSample s;
  tc.build_track_object(word_pos(wx, 0, wz), word_pos(wx, 0, wz), &s);
  EXPECT_EQ(s.plane_index, 0);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Grass));
  EXPECT_EQ(s.wall_index, -1);
  // Grass adds the ((worldx ^ worldz) >> 8) & 1 checkerboard, which is 0 at a
  // cell centre (both coordinates are odd multiples of 512).
  EXPECT_EQ(s.terrain_height, ((wx ^ wz) >> 8) & 1);
}

TEST(track_collision_build_object_road_is_paved) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 3, 4, 4);  // item 4: straight road, physical model 1
  tc.set_track(&t);

  const int wx = 3 * 1024 + 512;
  const int wz = 4 * 1024 + 512;
  CollisionSample s;
  tc.build_track_object(word_pos(wx, 0, wz), word_pos(wx, 0, wz), &s);
  EXPECT_EQ(s.physical_model, 1);
  EXPECT_EQ(s.plane_index, 0);
  // Pavement: surface type 1 and the +2 non-grass height offset.
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));
  EXPECT_EQ(s.terrain_height, 2);
}

TEST(track_collision_build_object_ramp_selects_slope_plane) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 5, 6, 58);  // item 58: ramp, rot 0, physical model 16
  tc.set_track(&t);

  const int wx = 5 * 1024 + 512;
  const int wz = 6 * 1024 + 512;
  CollisionSample s;
  tc.build_track_object(word_pos(wx, 0, wz), word_pos(wx, 0, wz), &s);
  EXPECT_EQ(s.physical_model, 16);
  // Group 3 (ramp) with no orientation offset -> plane 12.
  EXPECT_EQ(s.plane_index, 12);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));
}

TEST(track_collision_build_object_hill_terrain_slope) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_terrain(&t, 4, 8, 7);  // terrain 7, variant 0 -> uphill slope plane
  tc.set_track(&t);

  const int wx = 4 * 1024 + 512;
  const int wz = 8 * 1024 + 512;
  CollisionSample s;
  tc.build_track_object(word_pos(wx, 0, wz), word_pos(wx, 0, wz), &s);
  // code_bto_blank hill parsing: terrain 7 with planindex 0 -> planindex 3,
  // orient 0 -> plane 12.
  EXPECT_EQ(s.plane_index, 12);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Grass));
}

TEST(track_collision_build_object_elevated_terrain_adds_height) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_terrain(&t, 2, 2, 6);  // elevated plateau -> +450
  tc.set_track(&t);

  const int wx = 2 * 1024 + 512;
  const int wz = 2 * 1024 + 512;
  CollisionSample s;
  tc.build_track_object(word_pos(wx, 350, wz), word_pos(wx, 350, wz), &s);
  EXPECT_EQ(s.plane_index, 0);
  EXPECT_TRUE(s.terrain_height >= 450);
}

TEST(track_collision_build_object_bound_track_matches_explicit_track) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 3, 4, 4);
  tc.set_track(&t);

  const int wx = 3 * 1024 + 512;
  const int wz = 4 * 1024 + 512;
  CollisionSample a, b;
  tc.build_track_object(word_pos(wx, 0, wz), word_pos(wx, 0, wz), &a);
  tc.build_track_object_from_track(t, word_pos(wx, 0, wz), word_pos(wx, 0, wz), &b);
  EXPECT_EQ(a.plane_index, b.plane_index);
  EXPECT_EQ(a.surface_type, b.surface_type);
  EXPECT_EQ(a.terrain_height, b.terrain_height);
}

// ---------------------------------------------------------------------------
// Slalom and scenery walls (code_bto_slalom / code_bto_barn / ...)
// ---------------------------------------------------------------------------
TEST(track_collision_build_object_slalom_gate_wall) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 3, 4, 115);  // item 115: slalom, rot 0, physical model 34
  tc.set_track(&t);

  const int cx = 3 * 1024 + 512;
  const int cz = 4 * 1024 + 512;
  CollisionSample s;
  // vx = 0x30 is inside the post's x band, vz = -0x100 inside the z band; the
  // next z falls past the band's lower edge, selecting wall 0x91.
  tc.build_track_object(word_pos(cx + 0x30, 0, cz - 0x100),
                        word_pos(cx + 0x30, 0, cz - 0x200), &s);
  EXPECT_EQ(s.physical_model, 34);
  EXPECT_EQ(s.wall_index, 0x91);
  EXPECT_EQ(s.wall_height, 0x2A);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));

  // Driving through the gap (next z still inside the band) hits nothing.
  tc.build_track_object(word_pos(cx + 0x30, 0, cz - 0x100),
                        word_pos(cx + 0x30, 0, cz - 0x100), &s);
  EXPECT_EQ(s.wall_index, -1);
}

TEST(track_collision_build_object_scenery_building_wall) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 5, 6, 159);  // item 159: barn, physical model 65
  tc.set_track(&t);

  const int cx = 5 * 1024 + 512;
  const int cz = 6 * 1024 + 512;
  CollisionSample s;
  // Wheel at the cell centre; a next position past the -z face -> wall 0xA1.
  tc.build_track_object(word_pos(cx, 0, cz), word_pos(cx, 0, cz - 0x100), &s);
  EXPECT_EQ(s.physical_model, 65);
  EXPECT_EQ(s.wall_index, 0xA1);
  EXPECT_EQ(s.wall_height, 0x1A9);
}

TEST(track_collision_build_object_scenery_pine_has_no_wall) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 5, 6, 153);  // item 153: pine, physical model 71 (blank handler)
  tc.set_track(&t);

  const int cx = 5 * 1024 + 512;
  const int cz = 6 * 1024 + 512;
  CollisionSample s;
  tc.build_track_object(word_pos(cx, 0, cz), word_pos(cx, 0, cz - 0x100), &s);
  EXPECT_EQ(s.physical_model, 71);
  EXPECT_EQ(s.wall_index, -1);
}

// ---------------------------------------------------------------------------
// Corkscrew walls / planes (code_bto_corkLr / code_bto_corkUd*)
// ---------------------------------------------------------------------------
TEST(track_collision_build_object_corkscrew_lr_wall) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 3, 4, 85);  // item 85: corkscrew l/r, rot 0, physical model 35
  tc.set_track(&t);

  // multi_tile 1 -> the element centre's z is the cell's lower edge (row*1024).
  const int cx = 3 * 1024 + 512;
  const int cz = 4 * 1024;
  CollisionSample s;
  tc.build_track_object(word_pos(cx, 0, cz), word_pos(cx, 0, cz), &s);
  EXPECT_EQ(s.physical_model, 35);
  EXPECT_EQ(s.plane_index, 0);
  EXPECT_EQ(s.wall_index, 0xB9);
  EXPECT_EQ(s.wall_height, 0x75);
}

TEST(track_collision_build_object_corkscrew_ud_wall_and_plane) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 3, 4, 198);  // item 198: cork u/d A, rot 0, physical model 32
  tc.set_track(&t);

  // multi_tile 3 -> x centre is (col+1)*1024, z centre is row*1024.
  const int cx = 4 * 1024;
  const int cz = 4 * 1024;
  CollisionSample s;
  // vz > 0 with a raised wheel takes the walled branch: wall 0x63, plane 0x68.
  tc.build_track_object(word_pos(cx - 0x200, 400, cz + 0x100),
                        word_pos(cx - 0x200, 400, cz + 0x100), &s);
  EXPECT_EQ(s.physical_model, 32);
  EXPECT_EQ(s.wall_index, 0x63);
  EXPECT_EQ(s.plane_index, 0x68 * 4);
  EXPECT_EQ(s.wall_height, 0x2A);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));

  // The same spot at ground level falls to the spiral branch: a plane and a
  // radial wall, both non-trivial.
  tc.build_track_object(word_pos(cx - 0x200, 0, cz + 0x100),
                        word_pos(cx - 0x200, 0, cz + 0x100), &s);
  EXPECT_TRUE(s.plane_index != 0);
  EXPECT_TRUE(s.wall_index >= 0);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));
}

TEST(track_collision_build_object_tunnel_bore_and_wall) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 3, 4, 66);  // item 66: tunnel, rot 0, physical model 28
  tc.set_track(&t);

  const int cx = 3 * 1024 + 512;
  const int cz = 4 * 1024 + 512;
  CollisionSample s;
  // At ceiling height the wheel rides the flat bore plane 0x85.
  tc.build_track_object(word_pos(cx, 0x90, cz), word_pos(cx, 0x90, cz), &s);
  EXPECT_EQ(s.physical_model, 28);
  EXPECT_EQ(s.plane_index, 0x85 * 4);
  EXPECT_EQ(s.wall_index, -1);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));

  // Near ground with the wheel inside the right x band and the next position
  // beyond the far z edge: the near wall 0x9A.
  tc.build_track_object(word_pos(cx + 0x100, 0, cz),
                        word_pos(cx + 0x100, 0, cz - 0x200), &s);
  EXPECT_EQ(s.wall_index, 0x9A);
  EXPECT_EQ(s.wall_height, 0x90);
}

TEST(track_collision_build_object_pipe_entrance_plane_and_wall) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 3, 4, 70);  // item 70: pipe entrance, rot 0, physical model 29
  tc.set_track(&t);

  const int cx = 3 * 1024 + 512;
  const int cz = 4 * 1024 + 512;
  CollisionSample s;
  // Near the centre the entrance selects its opening plane 0x46.
  tc.build_track_object(word_pos(cx + 0x10, 0, cz), word_pos(cx + 0x10, 0, cz), &s);
  EXPECT_EQ(s.physical_model, 29);
  EXPECT_EQ(s.plane_index, 0x46 * 4);
  EXPECT_EQ(s.wall_index, -1);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));

  // The leading lip is a wall (0x9F on the positive-x side).
  tc.build_track_object(word_pos(cx + 0x20, 0, cz),
                        word_pos(cx + 0x80, 0, cz), &s);
  EXPECT_EQ(s.wall_index, 0x9F);
  EXPECT_EQ(s.wall_height, 0x97);
}

TEST(track_collision_build_object_pipe_and_half_pipe) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }

  const int cx = 3 * 1024 + 512;
  const int cz = 4 * 1024 + 512;
  CollisionSample s;

  Track tp = empty_track();
  set_item(&tp, 3, 4, 68);  // item 68: pipe, rot 0, physical model 30
  tc.set_track(&tp);
  // Straight through the pipe: the low left plane 0x39.
  tc.build_track_object(word_pos(cx + 0x10, 0, cz + 0x10),
                        word_pos(cx + 0x10, 0, cz + 0x10), &s);
  EXPECT_EQ(s.physical_model, 30);
  EXPECT_EQ(s.plane_index, 0x39 * 4);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));

  // A wheel moving past the pipe mouth hits the entry wall 0x9B.
  tc.build_track_object(word_pos(cx + 0x10, 0, cz),
                        word_pos(cx + 0xA4, 0, cz), &s);
  EXPECT_EQ(s.wall_index, 0x9B);
  EXPECT_EQ(s.wall_height, 0x97);

  Track th = empty_track();
  set_item(&th, 3, 4, 83);  // item 83: half pipe, rot 0, physical model 31
  tc.set_track(&th);
  // Inside the half-pipe channel: pavement plane 0x45, no side wall.
  tc.build_track_object(word_pos(cx + 0x10, 0, cz + 0x10),
                        word_pos(cx + 0x10, 0, cz + 0x10), &s);
  EXPECT_EQ(s.physical_model, 31);
  EXPECT_EQ(s.plane_index, 0x45 * 4);
  EXPECT_EQ(s.wall_index, -1);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));
}

TEST(track_collision_build_object_loop_surface_and_base) {
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no Stunts asset dir / GAME.PRE)\n");
    return;
  }
  Track t = empty_track();
  set_item(&t, 3, 4, 64);  // item 64: loop, rot 0, physical model 27
  tc.set_track(&t);

  // multi_tile 1 -> z centre is row*1024, x centre is col*1024 + 512.
  const int cx = 3 * 1024 + 512;
  const int cz = 4 * 1024;
  CollisionSample s;
  // On the loop's inner face the interpolated x band picks riding plane 0x2E.
  tc.build_track_object(word_pos(cx - 0x180, 0, cz + 0x100),
                        word_pos(cx - 0x180, 0, cz + 0x100), &s);
  EXPECT_EQ(s.physical_model, 27);
  EXPECT_EQ(s.plane_index, 0x2E * 4);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));

  // Right at the loop centre the wheel sits on the flat loop base pavement.
  tc.build_track_object(word_pos(cx, 0, cz), word_pos(cx, 0, cz), &s);
  EXPECT_EQ(s.plane_index, 0);
  EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));
}

// ---------------------------------------------------------------------------
// Real track sanity check
// ---------------------------------------------------------------------------
TEST(track_collision_build_object_real_track_road_cell) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir)\n");
    return;
  }
  TrackCollision tc;
  if (!load_collision(&tc)) {
    std::printf("  (skipped: no GAME.PRE)\n");
    return;
  }
  Track t = Track::load(path_join(dir, "DEFAULT.TRK"));
  tc.set_track(&t);

  // Scan for a straight-road element (item 4/5) and confirm it resolves to the
  // flat plane and a paved surface.
  bool found = false;
  for (int y = 0; y < Track::kSize && !found; ++y) {
    for (int x = 0; x < Track::kSize && !found; ++x) {
      const uint8_t item = t.item(x, y);
      if (item != 4 && item != 5) continue;
      const int wx = x * 1024 + 512;
      const int wz = y * 1024 + 512;
      CollisionSample s;
      tc.build_track_object(word_pos(wx, 0, wz), word_pos(wx, 0, wz), &s);
      EXPECT_EQ(s.plane_index, 0);
      EXPECT_EQ(s.surface_type, static_cast<int8_t>(CarSurfaceType::Paved));
      found = true;
    }
  }
  EXPECT_TRUE(found);
}
