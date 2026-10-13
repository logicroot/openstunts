// Crash debris simulation, ported from restunts asmorig/seg001.asm:
//   state_op_unk @9215  the burst spawn
//   sub_19BA0    @9386  the per-frame motion
//
// The arithmetic follows the original's 16/32-bit integer behaviour exactly:
// positions are 32-bit 1/64-world-unit accumulators, the tumble and heading
// fields are 16-bit angles in 1/1024-turn units that wrap, and the vertical
// speed is a 16-bit value gravity erodes.

#include "physics/car_debris.h"

#include <algorithm>

#include "physics/fixed_math.h"

namespace s3d::physics {

void DebrisField::clear() {
  for (DebrisPiece& p : pieces) p = DebrisPiece{};
}

bool DebrisField::any_active() const {
  for (const DebrisPiece& p : pieces) {
    if (p.active()) return true;
  }
  return false;
}

int DebrisField::live_count() const {
  int n = 0;
  for (const DebrisPiece& p : pieces) {
    if (p.active()) ++n;
  }
  return n;
}

int spawn_debris(DebrisField* field, int owner, int16_t car_yaw, int16_t extra,
                 KevinRandom* rng) {
  if (field == nullptr) return 0;

  // state_op_unk's prologue: a car burst fills the full circle, a track-object
  // burst a narrow forward arc.
  const bool is_car = owner < kDebrisOwnerTrackObjectBase;
  const int16_t base_yaw =
      is_car ? car_yaw : static_cast<int16_t>(car_yaw - 0x60);
  const int32_t spread = is_car ? 0x400 : 0xC0;
  const int max_pieces = is_car ? kDebrisCarPieces : kDebrisSceneryPieces;
  const int shape_base = is_car ? owner * 4 + 4 : 0;
  // var_8: the launch-speed multiplier for the vertical speed (6 for a car,
  // 1 for scenery; field_3BE = (var_8 * field_38E) >> 2).
  const int vy_num = is_car ? 6 : 1;

  // var_12: fill up to var_E free slots.
  int free_slots = 0;
  for (const DebrisPiece& p : field->pieces) {
    if (!p.active()) ++free_slots;
  }
  const int count = std::min(free_slots, max_pieces);

  int spawned = 0;
  for (DebrisPiece& p : field->pieces) {
    if (spawned >= count) break;
    if (p.active()) continue;  // the original keeps existing pieces

    p = DebrisPiece{};
    p.owner = static_cast<uint8_t>(owner);
    p.shape = static_cast<uint8_t>((spawned & 3) + shape_base);
    p.spin_z = static_cast<int16_t>(rng->next() * 4);
    p.spin_x = static_cast<int16_t>(rng->next() * 4);
    // field_35E: (n * spread) / count evenly spreads the burst, offset by the
    // base heading. The original's 16-bit `add` then `and ah, 3` is the same as
    // a mask to the 0x400-unit circle.
    const int32_t quotient = (static_cast<int32_t>(spawned) * spread) / count;
    p.angle = static_cast<int16_t>((quotient + base_yaw) & 0x3FF);

    // field_38E: ((rand*6) >> 2) + extra + 0x180.
    const int r = rng->next();
    const int speed = ((r * 6) >> 2) + extra + kDebrisBaseSpeed;
    p.speed = static_cast<int16_t>(speed);
    // field_3BE: the 16-bit `imul` low word, shifted right twice.
    const int16_t product = static_cast<int16_t>(static_cast<uint16_t>(vy_num * speed));
    p.vy = static_cast<int16_t>(product >> 2);
    ++spawned;
  }
  return spawned;
}

void update_debris(DebrisField* field, const CarState& player, int frames_per_sec) {
  if (field == nullptr) return;

  for (DebrisPiece& p : field->pieces) {
    if (!p.active()) continue;

    // The launch direction: mat_rot_zxy(0, 0, angle, 1) applied to
    // (0, 0, speed). Only the rotated x and z move the piece; the y component
    // is ignored (gravity owns the vertical axis).
    const fixed::Mat3 rot = fixed::mat_rot_zxy(0, 0, p.angle, 1);
    fixed::Vec3 out;
    fixed::mat_mul_vector(fixed::Vec3{0, 0, p.speed}, rot, &out);
    p.x += out.x;
    p.z += out.z;

    p.vy = static_cast<int16_t>(p.vy - kDebrisGravity);
    p.y += p.vy;
    if (frames_per_sec == 10) {
      // The original doubles gravity in the crash slow-motion (10 Hz) mode.
      p.vy = static_cast<int16_t>(p.vy - kDebrisGravity);
      p.y += p.vy;
    }

    // Retire the piece once it is below sea level. The original always adds the
    // *player's* height, even for the opponent's and the scenery's pieces.
    const uint32_t world_y = static_cast<uint32_t>(p.y) +
                             static_cast<uint32_t>(player.car_position.y);
    if (static_cast<int32_t>(world_y) < 0) {
      p.speed = 0;  // field_38E = 0 frees the slot
      continue;
    }

    // The surviving pieces keep tumbling (loc_19BB0).
    p.spin_z = static_cast<int16_t>(p.spin_z + kDebrisSpinStep);
    p.spin_x = static_cast<int16_t>(p.spin_x + kDebrisSpinStep);
  }
}

}  // namespace s3d::physics
