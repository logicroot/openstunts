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

// Appends `shape` transformed by `model`. Polygons are triangle-fanned; points
// are skipped. `color` is used for every primitive.
void append_shape(MeshBuilder& mb, const Shape3D& shape, const glm::mat4& model,
                  const glm::vec4& color);

// As above, but each primitive's color is looked up from its paint index.
//
// `material` selects which of the shape's paint slots to use (the original
// TRANSFORMEDSHAPE3D.material / shape3d_numpaints mechanism). A shape may carry
// several paint bytes per primitive (e.g. the windmill has 4); the slot is
// clamped to 0 when it is out of range, matching
// shape3d_transformed_shape_op. Most shapes have a single slot.
//
// When `line_out` is non-null, primitives of kind Line are appended to it as
// vertex pairs suitable for GL_LINES (used by the flashing diner sign). When it
// is null, lines are skipped as before.
void append_shape(MeshBuilder& mb, const Shape3D& shape, const glm::mat4& model,
                  const PaintColorFn& color_for, int material = 0,
                  MeshBuilder* line_out = nullptr);

// Convenience: builds an identity-transform model that translates to (tx, ty, tz)
// and rotates `yaw_deg` degrees about the world Y axis.
glm::mat4 shape_model(const glm::vec3& translation, float yaw_deg);

// Default paint-index -> colour mapping for track/scenery shapes. The original
// material table is not loaded yet, so common indices are mapped to sensible
// modern colours and everything else gets a stable grey.
glm::vec4 track_paint_color(uint8_t paint);

}  // namespace s3d::world
