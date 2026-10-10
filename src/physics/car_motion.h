#pragma once
// Reduced wheel-pose integrator, ported from restunts `stateply.c`
// (prepare_wheel_motion / prepare_wheel_rotation / resolve_wheel_contacts /
// apply_wheel_suspension / derive_car_pose_from_wheels) and `math.c`
// (transform_wheel_travel_to_world).
//
// The original collides wheels against dozens of cached track planes. The
// reduced model uses a caller-supplied 2D ground sampler by default; when a
// loaded TrackCollision is passed it instead resolves the active track plane
// under each wheel (ramps, banked roads, elevated terrain) via
// build_track_object / plane_signed_distance and travels along it via
// transform_wheel_travel_to_world. Wall/loop collisions are still deferred.

#include <cstdint>
#include <functional>

#include "assets/car.h"
#include "physics/car_state.h"

namespace s3d::physics {

class TrackCollision;

// Ground sample under a wheel. `height` is in world units (matching the 16-bit
// "word" coordinates the renderer uses); `surface` is a kCarSurface* id.
struct GroundSample {
  int height = 0;
  int surface = kCarSurfaceGrass;
};

// Samples the ground at a world position. `wx`, `wz` are world units.
using GroundSampler = std::function<GroundSample(float wx, float wz)>;

// Advances the car one physics tick at `frames_per_sec` (10 or 20 Hz). Reads
// and updates engine/steering/grip state that must be refreshed beforehand
// (update_car_speed, update_player_steering, update_grip). Commits
// car_position/car_rotate and refreshes car_surfaceWhl[], car_sumSurf*,
// car_wheel_vertical_speed[], car_suspension_deflection[] and
// car_wheel_contact_positions[].
void step_car_motion(CarState* car, const CarSpec& spec, int frames_per_sec,
                     const GroundSampler& ground);
void step_car_motion(CarState* car, const CarSpec& spec, int frames_per_sec,
                     const GroundSampler& ground, const TrackCollision* collision);

// Per-wheel suspension spring (restunts physics_collision.c
// update_wheel_suspension). `contact_delta` is the pre-correction signed
// distance of the wheel to the ground, in world units. Exposed for tests.
int16_t update_wheel_suspension(CarState* car, int16_t wheel_index, int16_t contact_delta);

}  // namespace s3d::physics