#include "game/race_state.h"

#include "physics/fixed_math.h"
#include "physics/wrap_math.h"
#include "world/world_units.h"

namespace s3d::game {

namespace {

// seg004.asm maps filler bytes before comparing against the start/finish set:
// element >= 0xFD becomes blank, 0xB6..0xFC becomes 4 (a plain road tile).
uint8_t normalize_element(uint8_t e) {
  if (e >= 0xFD) return 0;
  if (e >= 0xB6) return 4;
  return e;
}

std::string pad_number(int value, int width, char fill) {
  std::string s = std::to_string(value);
  if (static_cast<int>(s.size()) < width) {
    s.insert(0, static_cast<size_t>(width) - s.size(), fill);
  }
  return s;
}

}  // namespace

int start_finish_angle(uint8_t element) {
  // seg004.asm loc_2082C..loc_20952 (element byte -> track direction).
  switch (element) {
    case 0x01:
    case 0x86:
    case 0x93:
      return 0x000;
    case 0x87:
    case 0x94:
    case 0xB3:
      return 0x200;
    case 0x88:
    case 0x95:
    case 0xB4:
      return 0x100;
    case 0x89:
    case 0x96:
    case 0xB5:
      return 0x300;
    default:
      return -1;
  }
}

StartFinish find_start_finish(const Track& track) {
  StartFinish sf;
  for (int row = 0; row < Track::kSize; ++row) {
    for (int col = 0; col < Track::kSize; ++col) {
      const int angle = start_finish_angle(normalize_element(track.item(col, row)));
      if (angle < 0) continue;
      if (sf.found) {
        // The original aborts track setup with `many_sf`; report it so the
        // caller can fall back to a default spawn.
        sf.multiple = true;
        return sf;
      }
      sf.found = true;
      sf.col = col;
      sf.row = row;
      sf.angle = angle;
    }
  }
  return sf;
}

int finish_line_distance(int32_t car_x_long, int32_t car_z_long,
                         const StartFinish& sf) {
  if (!sf.found) return 1;
  // trackcenterpos[row] / trackcenterpos2[col] are the cell centres; car
  // positions are 1/64 world units (the original shifts them right by 6).
  const int center_x = sf.col * static_cast<int>(world::kSquareSize) +
                       static_cast<int>(world::kSquareSize / 2);
  const int center_z = sf.row * static_cast<int>(world::kSquareSize) +
                       static_cast<int>(world::kSquareSize / 2);
  const int16_t car_x = fixed::wrap16(car_x_long >> 6);
  const int16_t car_z = fixed::wrap16(car_z_long >> 6);
  const int16_t dz = fixed::wrap16(center_z - static_cast<int>(car_z));
  const int16_t dx = fixed::wrap16(center_x - static_cast<int>(car_x));
  const int16_t c = fixed::cos_fast(static_cast<unsigned short>(sf.angle));
  const int16_t s = fixed::sin_fast(static_cast<unsigned short>(sf.angle));
  return physics::wrap_add(fixed::multiply_and_scale(c, dz),
                           fixed::multiply_and_scale(s, dx));
}

std::string format_race_time(int frames, int frames_per_sec, bool hundredths) {
  if (frames < 0) frames = 0;
  const int fps = frames_per_sec > 0 ? frames_per_sec : 1;
  const int per_minute = 60 * fps;
  const int minutes = frames / per_minute;
  int rem = frames - per_minute * minutes;
  const int seconds = rem / fps;
  rem -= fps * seconds;

  // print_int_as_string_maybe: minutes left-padded with spaces to two columns,
  // seconds/padded with zeros; the hundredths field is (100 / fps) * frames.
  std::string out = pad_number(minutes, 2, ' ') + ":" + pad_number(seconds, 2, '0');
  if (hundredths) {
    out += "." + pad_number((100 / fps) * rem, 2, '0');
  }
  return out;
}

}  // namespace s3d::game
