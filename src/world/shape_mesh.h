#pragma once
// Converts a `Shape3D` (raw P3S geometry) into triangles appended to a
// `MeshBuilder`. Shape vertices are in native world units (one grid square is
// 1024 units), so callers supply a model matrix that positions/orients the
// shape within the world.

#include <glm/glm.hpp>

#include <cstdint>
#include <functional>

#include "assets/shape3d.h"
#include "world/mesh_builder.h"

namespace s3d::world {

// Color chosen per primitive paint index (one entry per material). Returning a
// color with alpha 0 skips the primitive.
using PaintColorFn = std::function<glm::vec4(uint8_t)>;

// Appends `shape` transformed by `model`. Polygons are triangle-fanned, lines
// are skipped (they are usually wireframe detail), points are skipped.
// `color` is used for every primitive.
void append_shape(MeshBuilder& mb, const Shape3D& shape, const glm::mat4& model,
                  const glm::vec4& color);

// As above, but each primitive's color is looked up from its paint index.
void append_shape(MeshBuilder& mb, const Shape3D& shape, const glm::mat4& model,
                  const PaintColorFn& color_for);

// Convenience: builds an identity-transform model that translates to (tx, ty, tz)
// and rotates `yaw_deg` degrees about the world Y axis.
glm::mat4 shape_model(const glm::vec3& translation, float yaw_deg);

}  // namespace s3d::world
