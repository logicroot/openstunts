#include "world/shape_mesh.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <vector>

namespace s3d::world {

namespace {

glm::vec3 transform_point(const glm::mat4& m, const Vec3i& v) {
  return glm::vec3(m * glm::vec4(static_cast<float>(v.x), static_cast<float>(v.y),
                                 static_cast<float>(v.z), 1.0f));
}

glm::vec3 face_normal(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
  const glm::vec3 n = glm::cross(b - a, c - a);
  const float len = glm::length(n);
  return (len > 1e-6f) ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
}

// Smallest circle enclosing a set of 2D points. The wheel primitive only ever
// has six rim points, so a brute force over every candidate circle (each pair's
// diameter circle, each triple's circumcircle) is exact and trivially cheap.
struct Circle2 {
  glm::vec2 center{0.0f};
  float radius = -1.0f;  // < 0 means "no circle"
};

bool circle_covers(const std::vector<glm::vec2>& pts, const Circle2& c) {
  for (const glm::vec2& p : pts) {
    if (glm::distance(p, c.center) > c.radius + 1e-4f) return false;
  }
  return true;
}

Circle2 minimal_enclosing_circle(const std::vector<glm::vec2>& pts) {
  Circle2 best;
  const auto consider = [&](const Circle2& c) {
    if (c.radius <= 0.0f) return;
    if (best.radius >= 0.0f && c.radius >= best.radius) return;
    if (!circle_covers(pts, c)) return;
    best = c;
  };
  for (size_t i = 0; i < pts.size(); ++i) {
    for (size_t j = i + 1; j < pts.size(); ++j) {
      consider({0.5f * (pts[i] + pts[j]), 0.5f * glm::distance(pts[i], pts[j])});
    }
  }
  for (size_t i = 0; i < pts.size(); ++i) {
    for (size_t j = i + 1; j < pts.size(); ++j) {
      for (size_t k = j + 1; k < pts.size(); ++k) {
        const glm::vec2& a = pts[i];
        const glm::vec2& b = pts[j];
        const glm::vec2& c = pts[k];
        const float d = 2.0f * (a.x * (b.y - c.y) + b.x * (c.y - a.y) +
                                c.x * (a.y - b.y));
        if (std::abs(d) < 1e-5f) continue;  // collinear
        const float a2 = a.x * a.x + a.y * a.y;
        const float b2 = b.x * b.x + b.y * b.y;
        const float c2 = c.x * c.x + c.y * c.y;
        const glm::vec2 cc{(a2 * (b.y - c.y) + b2 * (c.y - a.y) + c2 * (a.y - b.y)) / d,
                           (a2 * (c.x - b.x) + b2 * (a.x - c.x) + c2 * (b.x - a.x)) / d};
        consider({cc, glm::distance(cc, a)});
      }
    }
  }
  return best;
}

// Appends `pts` as a triangle fan. `normal_override`, when given, replaces the
// face normal derived from the first three vertices. The flat markings that lie
// on a surface use it so that a marking authored with the opposite winding is
// still shaded like the surface it is painted on.
void append_polygon(MeshBuilder& mb, const std::vector<glm::vec3>& pts,
                    const glm::vec4& color,
                    const glm::vec3* normal_override = nullptr) {
  if (pts.size() < 3) return;
  const glm::vec3 n =
      normal_override != nullptr ? *normal_override : face_normal(pts[0], pts[1], pts[2]);
  // Triangle fan.
  for (size_t i = 1; i + 1 < pts.size(); ++i) {
    mb.add_triangle(pts[0], pts[i], pts[i + 1], color, n);
  }
}

// A polygon's plane, size and extent, used to recognise the flat markings an
// element paints on top of one of its own (coincident) surfaces.
struct PolyInfo {
  glm::vec3 normal{0.0f, 1.0f, 0.0f};  // unit
  float area = 0.0f;
  float plane_d = 0.0f;  // dot(normal, first vertex)
  glm::vec3 centroid{0.0f};
  glm::vec3 bb_min{0.0f};
  glm::vec3 bb_max{0.0f};
};

// Fills `out` from a planar polygon (Newell's method gives both the area and a
// consistent normal). Returns false for a degenerate (zero-area) polygon.
bool polygon_info(const std::vector<glm::vec3>& pts, PolyInfo* out) {
  if (pts.size() < 3) return false;
  glm::vec3 n(0.0f);
  for (size_t i = 0; i < pts.size(); ++i) {
    n += glm::cross(pts[i], pts[(i + 1) % pts.size()]);
  }
  const float len = glm::length(n);
  if (len < 1e-6f) return false;
  out->normal = n / len;
  out->area = 0.5f * len;
  out->plane_d = glm::dot(out->normal, pts[0]);
  out->centroid = glm::vec3(0.0f);
  out->bb_min = out->bb_max = pts[0];
  for (const glm::vec3& p : pts) {
    out->centroid += p;
    out->bb_min = glm::min(out->bb_min, p);
    out->bb_max = glm::max(out->bb_max, p);
  }
  out->centroid /= static_cast<float>(pts.size());
  return true;
}

// A polygon counts as a marking painted on a surface when a much larger polygon
// of the same shape -- one at least this many times its area -- lies under it...
constexpr float kDecalHostAreaRatio = 4.0f;
// ...with the marking's centre inside that surface's bounds and on its plane to
// within this many world units (one grid square is 1024). The authored markings
// are coincident with their surface, and on a curved piece (a banked turn) they
// straddle several of its flat panels, so the tolerance has to allow for that.
constexpr float kDecalPlaneTolerance = 16.0f;

// Returns the index of the polygon `poly` lies on -- a much larger, coplanar
// polygon of the same shape -- or -1 when it lies on none.
int find_surface_host(const PolyInfo& poly, const std::vector<PolyInfo>& polys,
                      const std::vector<char>& measured, size_t self) {
  for (size_t i = 0; i < polys.size(); ++i) {
    if (i == self || !measured[i]) continue;
    const PolyInfo& host = polys[i];
    if (host.area < kDecalHostAreaRatio * poly.area) continue;
    if (std::abs(glm::dot(host.normal, poly.centroid) - host.plane_d) > kDecalPlaneTolerance) {
      continue;
    }
    if (poly.centroid.x < host.bb_min.x || poly.centroid.x > host.bb_max.x) continue;
    if (poly.centroid.y < host.bb_min.y || poly.centroid.y > host.bb_max.y) continue;
    if (poly.centroid.z < host.bb_min.z || poly.centroid.z > host.bb_max.z) continue;
    return static_cast<int>(i);
  }
  return -1;
}

}  // namespace

void append_shape(MeshBuilder& mb, const Shape3D& shape, const glm::mat4& model,
                  const glm::vec4& color) {
  append_shape(mb, shape, model, [&color](uint8_t) { return color; });
}

void append_shape(MeshBuilder& mb, const Shape3D& shape, const glm::mat4& model,
                  const PaintColorFn& color_for, int material, MeshBuilder* line_out,
                  MeshBuilder* decal_out) {
  // Pre-transform all vertices once.
  std::vector<glm::vec3> pts(shape.vertices.size());
  for (size_t i = 0; i < shape.vertices.size(); ++i) {
    pts[i] = transform_point(model, shape.vertices[i]);
  }

  // Decal routing only makes sense when the caller gave a distinct buffer.
  const bool route_decals = decal_out != nullptr && decal_out != &mb;

  // Classify each polygon once: small polygons lying on a larger one of the same
  // shape are the markings an element paints over its own surface. (The originals
  // draw them after the surface, so the marking wins; a z-buffer z-fights them.)
  std::vector<PolyInfo> info(shape.primitives.size());
  std::vector<char> measured(shape.primitives.size(), 0);
  std::vector<char> decal(shape.primitives.size(), 0);
  std::vector<int> decal_host(shape.primitives.size(), -1);
  if (route_decals) {
    for (size_t i = 0; i < shape.primitives.size(); ++i) {
      const ShapePrimitive& prim = shape.primitives[i];
      if (prim.kind != PrimKind::Polygon || prim.type == 12 || prim.indices.size() < 3) continue;
      std::vector<glm::vec3> poly;
      poly.reserve(prim.indices.size());
      for (uint8_t idx : prim.indices) {
        if (idx < pts.size()) poly.push_back(pts[idx]);
      }
      if (polygon_info(poly, &info[i])) measured[i] = 1;
    }
    for (size_t i = 0; i < shape.primitives.size(); ++i) {
      if (!measured[i]) continue;
      decal_host[i] = find_surface_host(info[i], info, measured, i);
      if (decal_host[i] >= 0) decal[i] = 1;
    }
  }

  for (size_t prim_index = 0; prim_index < shape.primitives.size(); ++prim_index) {
    const ShapePrimitive& prim = shape.primitives[prim_index];
    // Select this primitive's paint byte for the requested material slot. The
    // original clamps an out-of-range material to slot 0.
    int slot = material;
    if (slot < 0) slot = 0;
    uint8_t paint = 0;
    if (!prim.paint.empty()) {
      if (slot >= static_cast<int>(prim.paint.size())) slot = 0;
      paint = prim.paint[static_cast<size_t>(slot)];
    }
    const glm::vec4 color = color_for(paint);
    if (color.a <= 0.0f) continue;

    if (prim.kind == PrimKind::Line) {
      if (line_out == nullptr || prim.indices.size() < 2) continue;
      for (size_t i = 0; i + 1 < prim.indices.size(); i += 2) {
        const uint8_t a = prim.indices[i];
        const uint8_t b = prim.indices[i + 1];
        if (a >= pts.size() || b >= pts.size()) continue;
        line_out->add_vertex(pts[a], glm::vec3(0.0f, 1.0f, 0.0f), color);
        line_out->add_vertex(pts[b], glm::vec3(0.0f, 1.0f, 0.0f), color);
      }
      continue;
    }
    if (prim.kind != PrimKind::Polygon) continue;
    if (prim.indices.size() < 3) continue;

    // Primitive type 12 is the original's dedicated wheel ("_primtype_wheel",
    // primtypetab[12] = 3). It is authored as a six-vertex wedge: three
    // vertices on the tyre's inner face (ix[0..2]) and three on its outer face
    // (ix[3..5]), describing only the upper part of the wheel, so faithfully
    // drawn it reads as a flat triangle. Reconstruct the round wheel the data
    // implies instead: a low-poly cylinder about the axle whose radius is the
    // smallest circle enclosing the authored rim points. (A deliberate
    // deviation, agreed with the user so the wheels read as wheels.) The closed
    // cylinder also keeps the wheel opaque, which the original's two
    // camera-facing quads could not do in a z-buffered renderer.
    //
    // The authored material colour (palette 29, rgb(56,56,56) for every car) is
    // a dark grey, which the user reads as the tyre. So the outer band and
    // tread keep that colour, and the hub inside it is lightened into a grey
    // rim -- giving grey wheels wrapped in dark-grey tyres, as the user asked.
    if (prim.type == 12 && prim.indices.size() >= 6) {
      const std::vector<uint8_t>& ix = prim.indices;
      bool in_range = true;
      for (int k = 0; k < 6; ++k) {
        if (ix[static_cast<size_t>(k)] >= pts.size()) in_range = false;
      }
      if (in_range) {
        const glm::vec3 a0 = pts[ix[0]], a1 = pts[ix[1]], a2 = pts[ix[2]];
        const glm::vec3 b0 = pts[ix[3]], b1 = pts[ix[4]], b2 = pts[ix[5]];
        const glm::vec3 inner = (a0 + a1 + a2) / 3.0f;
        const glm::vec3 outer = (b0 + b1 + b2) / 3.0f;
        const glm::vec3 axle = outer - inner;
        const float width = glm::length(axle);
        if (width > 1e-3f) {
          const glm::vec3 axis = axle / width;
          // Any basis perpendicular to the axle spans the tyre's face.
          const glm::vec3 helper =
              (std::abs(axis.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
          const glm::vec3 u = glm::normalize(glm::cross(axis, helper));
          const glm::vec3 v = glm::cross(axis, u);
          const glm::vec3 rim[6] = {a0, a1, a2, b0, b1, b2};
          std::vector<glm::vec2> flat;
          flat.reserve(6);
          for (const glm::vec3& r : rim) {
            const glm::vec3 d = r - inner;
            flat.push_back(glm::vec2(glm::dot(d, u), glm::dot(d, v)));
          }
          const Circle2 circle = minimal_enclosing_circle(flat);
          if (circle.radius > 1e-3f) {
            const glm::vec3 centre = inner + circle.center.x * u + circle.center.y * v;
            // Two-tone wheel: the rim/hub is a lightened version of the tyre
            // (material) colour, so a wheel is a grey hub disc inside a
            // dark-grey tyre band and tread.
            const glm::vec4 tyre_color = color;
            const glm::vec3 rgb = glm::vec3(tyre_color);
            const glm::vec4 rim_color(
                glm::min(rgb * 2.4f + 0.10f, glm::vec3(1.0f)), tyre_color.a);
            std::vector<glm::vec3> tyre_in, tyre_out, rim_in, rim_out;
            const auto build_ring = [&](float radius, std::vector<glm::vec3>& in,
                                        std::vector<glm::vec3>& out) {
              in.reserve(kCarWheelSides);
              out.reserve(kCarWheelSides);
              for (int s = 0; s < kCarWheelSides; ++s) {
                const float angle =
                    6.28318530718f * static_cast<float>(s) / static_cast<float>(kCarWheelSides);
                const glm::vec3 rad = radius * (std::cos(angle) * u + std::sin(angle) * v);
                in.push_back(centre + rad);
                out.push_back(centre + axle + rad);
              }
            };
            build_ring(circle.radius, tyre_in, tyre_out);
            build_ring(circle.radius * kCarWheelRimRatio, rim_in, rim_out);

            // Grey hub discs capping either side of the rim.
            append_polygon(mb, rim_out, rim_color);  // outer hub
            const std::vector<glm::vec3> rim_in_rev(rim_in.rbegin(), rim_in.rend());
            append_polygon(mb, rim_in_rev, rim_color);  // inner hub
            for (int s = 0; s < kCarWheelSides; ++s) {
              const int t = (s + 1) % kCarWheelSides;
              // Dark-grey tyre: the sidewall bands around the hub...
              append_polygon(mb, {rim_out[s], tyre_out[s], tyre_out[t], rim_out[t]}, tyre_color);
              append_polygon(mb, {rim_in[s], tyre_in[s], tyre_in[t], rim_in[t]}, tyre_color);
              // ...and the tread between the two faces.
              append_polygon(mb, {tyre_in[s], tyre_out[s], tyre_out[t], tyre_in[t]}, tyre_color);
            }
            continue;
          }
        }
      }
      // Degenerate/unexpected wheel data: fall back to the closed prism of the
      // authored wedge rather than dropping the primitive.
      const int faces[5][4] = {
          {0, 1, 2, -1}, {5, 4, 3, -1},
          {0, 1, 4, 3}, {1, 2, 5, 4}, {2, 0, 3, 5},
      };
      for (const auto& f : faces) {
        std::vector<glm::vec3> poly;
        poly.reserve(4);
        for (int k = 0; k < 4 && f[k] >= 0; ++k) {
          const uint8_t idx = ix[static_cast<size_t>(f[k])];
          if (idx < pts.size()) poly.push_back(pts[idx]);
        }
        append_polygon(mb, poly, color);
      }
      continue;
    }

    std::vector<glm::vec3> poly;
    poly.reserve(prim.indices.size());
    for (uint8_t idx : prim.indices) {
      if (idx < pts.size()) poly.push_back(pts[idx]);
    }
    // A marking painted on one of the shape's own surfaces goes to the decal
    // layer, which the caller draws with a forward depth bias so it wins the
    // depth test against the surface, as it does in the original. It also takes
    // the surface's normal, because the authored markings are not always wound
    // the same way round as the surface (shape 30's fourth dash, for one), and a
    // reversed face would otherwise be lit as if it pointed away from the sun.
    const bool is_decal = route_decals && decal[prim_index];
    MeshBuilder& target = is_decal ? *decal_out : mb;
    const glm::vec3* normal_override = nullptr;
    if (is_decal && decal_host[prim_index] >= 0) {
      normal_override = &info[static_cast<size_t>(decal_host[prim_index])].normal;
    }
    append_polygon(target, poly, color, normal_override);
  }
}

glm::mat4 shape_model(const glm::vec3& translation, float yaw_deg) {
  glm::mat4 m = glm::translate(glm::mat4(1.0f), translation);
  return glm::rotate(m, glm::radians(yaw_deg), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec4 track_paint_color(uint8_t p) {
  switch (p) {
    case 19: return {0.30f, 0.30f, 0.33f, 1.0f};  // asphalt
    case 25: return {0.28f, 0.28f, 0.31f, 1.0f};
    case 28: return {0.26f, 0.26f, 0.29f, 1.0f};
    case 22: return {0.31f, 0.31f, 0.34f, 1.0f};
    case 23: return {0.35f, 0.35f, 0.38f, 1.0f};
    case 24: return {0.27f, 0.27f, 0.30f, 1.0f};
    case 21: case 27: case 30: return {0.92f, 0.92f, 0.90f, 1.0f};  // markings
    case 35: return {0.78f, 0.16f, 0.13f, 1.0f};  // red curb
    case 36: return {0.90f, 0.90f, 0.88f, 1.0f};  // white curb
    case 31: case 32: case 33: return {0.62f, 0.63f, 0.66f, 1.0f};  // rails
    case 10: return {0.42f, 0.27f, 0.14f, 1.0f};  // trunk
    case 98: return {0.22f, 0.46f, 0.19f, 1.0f};  // foliage
    case 99: return {0.16f, 0.37f, 0.14f, 1.0f};
    case 100: return {0.15f, 0.38f, 0.68f, 1.0f};  // water
    case 101: case 104: case 105: return {0.34f, 0.56f, 0.26f, 1.0f};  // grass
    case 102: case 103: return {0.29f, 0.50f, 0.22f, 1.0f};  // slope grass
    case 0: return {0.52f, 0.18f, 0.15f, 1.0f};  // barn
    case 4: return {0.80f, 0.78f, 0.72f, 1.0f};
    case 6: return {0.40f, 0.27f, 0.15f, 1.0f};
    case 12: return {0.58f, 0.44f, 0.29f, 1.0f};
    case 15: return {0.85f, 0.85f, 0.83f, 1.0f};
    default: {
      const float v = 0.40f + 0.012f * static_cast<float>(p % 9);
      return {v, v, v * 1.03f, 1.0f};
    }
  }
}

}  // namespace s3d::world
