// Car-vs-car collision tests (seg001.asm car_car_coll_detect_maybe and
// car_car_speed_adjust_maybe). Values use the real COUN/VETT collide_points so
// the box dimensions match the shipping cars.

#include <cstdint>

#include "physics/car_collision.h"
#include "physics/car_state.h"
#include "tiny_test.h"

using namespace s3d::physics;

namespace {

// COUN collide_points: {p0.x, p0.y, p1.x, p1.y}.
const CollisionPoints kCoun{20, 17, 54, 61};

CollisionFrame frame_at(int16_t x, int16_t y, int16_t z) {
  CollisionFrame f;
  f.pos[0] = x;
  f.pos[1] = y;
  f.pos[2] = z;
  f.rot[0] = 0;
  f.rot[1] = 0;
  f.rot[2] = 0;
  return f;
}

// A frame whose third rotation word is `yaw`, matching the opponent-style
// layout (rot[2] = car_rotate.x). Used to show rotation is applied.
CollisionFrame yaw_frame(int16_t x, int16_t y, int16_t z, int16_t yaw) {
  CollisionFrame f = frame_at(x, y, z);
  f.rot[2] = yaw;
  return f;
}

}  // namespace

TEST(car_collide_overlapping_is_true) {
  const CollisionFrame a = frame_at(1000, 10, 1000);
  const CollisionFrame b = frame_at(1000, 10, 1000);
  EXPECT_TRUE(car_car_collide(kCoun, a, kCoun, b));
  EXPECT_TRUE(car_car_collide(kCoun, b, kCoun, a));  // symmetric
}

TEST(car_collide_far_apart_is_false) {
  // Coarse test rejects anything past si = 61 + 61 = 122 on any single axis.
  const CollisionFrame a = frame_at(1000, 10, 1000);
  const CollisionFrame b = frame_at(6100, 10, 1000);
  EXPECT_TRUE(!car_car_collide(kCoun, a, kCoun, b));
}

TEST(car_collide_y_separation_is_false) {
  // The boxes only span y in [0, points[0].y] = [0, 17]; 18 apart misses.
  const CollisionFrame a = frame_at(1000, 10, 1000);
  const CollisionFrame b = frame_at(1000, 28, 1000);
  EXPECT_TRUE(!car_car_collide(kCoun, a, kCoun, b));
}

TEST(car_collide_refines_beyond_sphere) {
  // Delta = exactly si on x passes the coarse/sphere tests but the boxes
  // (half-width 20) do not actually touch.
  const CollisionFrame a = frame_at(1000, 0, 1000);
  const CollisionFrame b = frame_at(1122, 0, 1000);
  EXPECT_TRUE(!car_car_collide(kCoun, a, kCoun, b));
}

TEST(car_collide_rotation_flips_result) {
  // Offset along z so the leading corners overlap when aligned (half-depth 54
  // each) but stick out past the box once the other car is turned 90 degrees.
  const CollisionFrame a = frame_at(1000, 0, 1000);
  const CollisionFrame aligned = yaw_frame(1000, 0, 1100, 0);
  const CollisionFrame turned = yaw_frame(1000, 0, 1100, 0x100);
  EXPECT_TRUE(car_car_collide(kCoun, a, kCoun, aligned));
  EXPECT_TRUE(!car_car_collide(kCoun, a, kCoun, turned));
}

TEST(car_frame_player_vs_opponent_rotation) {
  CarState car;
  car.car_position.x = 6400;  // >>6 => 100
  car.car_position.y = 320;   // >>6 => 5
  car.car_position.z = -128;  // >>6 => -2
  car.car_rotate.z = 0x11;
  car.car_rotate.y = 0x22;
  car.car_rotate.x = 0x33;

  const CollisionFrame p = make_collision_frame(car, true);
  EXPECT_EQ(p.pos[0], 100);
  EXPECT_EQ(p.pos[1], 5);
  EXPECT_EQ(p.pos[2], -2);
  EXPECT_EQ(p.rot[0], static_cast<int16_t>(-0x11));
  EXPECT_EQ(p.rot[1], static_cast<int16_t>(-0x33));
  EXPECT_EQ(p.rot[2], static_cast<int16_t>(-0x22));

  const CollisionFrame o = make_collision_frame(car, false);
  EXPECT_EQ(o.rot[0], 0x11);
  EXPECT_EQ(o.rot[1], 0x22);
  EXPECT_EQ(o.rot[2], 0x33);
}

TEST(car_speed_adjust_soft_contact) {
  // Same heading and speed: the relative velocity is zero, so the collision is
  // clamped to the minimum magnitude (0xA) and does not crash.
  CarState a, b;
  a.car_rotate.x = 0;
  b.car_rotate.x = 0;
  a.car_actual_speed = 0x1000;
  b.car_actual_speed = 0x1000;

  EXPECT_TRUE(!car_car_speed_adjust(&a, &b));
  // decel = (0x300 * 10) >> 2 = 0x780; 0x1000 - 0x780 = 0x880.
  EXPECT_EQ(a.car_actual_speed, 0x880);
  EXPECT_EQ(b.car_actual_speed, 0x1000);  // only the ticked car loses speed
  EXPECT_EQ(a.car_rev_speed, 0x880);
  EXPECT_EQ(b.car_rev_speed, 0x1000);
  EXPECT_EQ(a.car_collision_latch, kCarCollisionLatchSet);
  EXPECT_EQ(b.car_collision_latch, kCarCollisionLatchSet);
  EXPECT_EQ(a.car_velocity_heading_offset, 0);
  EXPECT_EQ(b.car_velocity_heading_offset, 0);
}

TEST(car_speed_adjust_head_on_crashes) {
  // Opposite headings at 0x1000: relative magnitude 32 > 0x1E, so it crashes
  // and the ticked car's speed is fully absorbed.
  CarState a, b;
  a.car_rotate.x = 0;
  b.car_rotate.x = 0x200;
  a.car_actual_speed = 0x1000;
  b.car_actual_speed = 0x1000;

  EXPECT_TRUE(car_car_speed_adjust(&a, &b));
  EXPECT_EQ(a.car_actual_speed, 0);
  EXPECT_EQ(a.car_rev_speed, 0);
  EXPECT_EQ(b.car_actual_speed, 0x1000);
  EXPECT_EQ(a.car_velocity_heading_offset, 0x200);
  EXPECT_EQ(b.car_velocity_heading_offset, 0x200);
}

TEST(car_speed_adjust_heading_kick) {
  // Heading offsets are normalized to (-0x200, 0x200); zero speed keeps the
  // relative term at its 0xA floor so this stays a non-crash.
  CarState a, b;
  a.car_rotate.x = 0;
  b.car_rotate.x = 0x300;
  a.car_actual_speed = 0;
  b.car_actual_speed = 0;

  EXPECT_TRUE(!car_car_speed_adjust(&a, &b));
  EXPECT_EQ(a.car_velocity_heading_offset, static_cast<int16_t>(0xFF00));  // -0x100
  EXPECT_EQ(b.car_velocity_heading_offset, 0x100);
}
