#include <string>

#include "assets/car.h"
#include "assets/res_file.h"
#include "core/paths.h"
#include "tiny_test.h"

using namespace s3d;

TEST(res_file_parses_countach_headers) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  ResFile res;
  EXPECT_TRUE(res.load(path_join(dir, "CARCOUN.RES")));
  EXPECT_EQ(res.file_size(), 971u);
  EXPECT_EQ(res.chunks().size(), 4u);
  EXPECT_TRUE(res.find("simd") != nullptr);
  EXPECT_TRUE(res.find("gnam") != nullptr);
  EXPECT_TRUE(res.find("nope") == nullptr);
  EXPECT_EQ(res.read("simd").size(), 776u);
}

TEST(car_loads_countach_spec) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  CarSpec car;
  EXPECT_TRUE(CarSpec::load(path_join(dir, "CARCOUN.RES"), &car));
  EXPECT_EQ(car.num_gears, 5);
  EXPECT_EQ(car.car_mass, 33);
  EXPECT_EQ(car.grip, 210);
  EXPECT_EQ(car.surface_grip[0], 256);  // tarmac
  EXPECT_EQ(car.surface_grip[1], 192);  // dirt
  EXPECT_EQ(car.surface_grip[2], 64);   // ice
  EXPECT_EQ(car.surface_grip[3], 128);  // grass
  EXPECT_EQ(car.sliding_water, 0);      // the word after surfacegrip[3]
  EXPECT_EQ(car.car_height, 16);
  EXPECT_EQ(car.gear_ratios[1], 33339);
}

TEST(car_loads_lancia_and_lm002_grip_overrides) {
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  CarSpec lancia;
  EXPECT_TRUE(CarSpec::load(path_join(dir, "CARLANC.RES"), &lancia));
  EXPECT_EQ(lancia.car_mass, 25);

  CarSpec lm02;
  EXPECT_TRUE(CarSpec::load(path_join(dir, "CARLM02.RES"), &lm02));
  EXPECT_EQ(lm02.surface_grip[2], 160);  // ice override
  EXPECT_EQ(lm02.surface_grip[3], 192);  // grass override
}

TEST(car_countach_instrument_geometry) {
  // The cockpit instrument geometry in the SIMD chunk (seg005.asm
  // setup_car_shapes case 2): the analog speedometer/tachometer needle origins
  // and their byte-pair tip lists, plus the steering-dot positions.
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  CarSpec car;
  EXPECT_TRUE(CarSpec::load(path_join(dir, "CARCOUN.RES"), &car));
  EXPECT_EQ(car.speed_center.x, 25);
  EXPECT_EQ(car.speed_center.y, 19);   // not 0 (digital) / -1 (absent) -> analog
  EXPECT_EQ(car.speed_num_points, 84);
  EXPECT_EQ(car.rev_center.x, 107);
  EXPECT_EQ(car.rev_center.y, 19);
  EXPECT_EQ(car.rev_num_points, 71);
  // Byte-pair tip lists, indexed relative to their center.
  EXPECT_EQ(car.speed_points[0], 7);
  EXPECT_EQ(car.speed_points[1], 34);
  EXPECT_EQ(car.rev_points[0], 94);
  EXPECT_EQ(car.rev_points[1], 37);
  // 31 steering-dot positions, the first at the centered screen coordinate.
  EXPECT_EQ(car.steering_dots[0], 131);
  EXPECT_EQ(car.steering_dots[1], 135);
  EXPECT_EQ(car.steering_dots[30 * 2], 222);
  EXPECT_EQ(car.steering_dots[30 * 2 + 1], 191);
}

TEST(car_vetty_uses_digital_speedometer) {
  // Older cars show a digital speed readout: spdcenter.y == 0, so the analog
  // needle is skipped and `spdpoints` holds digit positions (seg005.asm
  // loc_233A2). The tachometer stays analog.
  const std::string dir = find_asset_dir();
  if (dir.empty()) {
    std::printf("  (skipped: no Stunts asset dir found)\n");
    return;
  }
  CarSpec vett;
  if (!CarSpec::load(path_join(dir, "CARVETT.RES"), &vett)) {
    std::printf("  (skipped: CARVETT.RES not found)\n");
    return;
  }
  EXPECT_EQ(vett.speed_center.x, 0);
  EXPECT_EQ(vett.speed_center.y, 0);
  EXPECT_EQ(vett.speed_num_points, 3);
  EXPECT_TRUE(vett.rev_num_points > 0);
}
