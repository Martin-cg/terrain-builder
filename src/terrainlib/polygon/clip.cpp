#include <array>
#include <cstdint>
#include <span>

#include "containers/FixedVector.h"
#include "mesh/geometry.h"
#include "mesh/WindingOrder.h"
#include "polygon/clip.h"

namespace polygon {

namespace {
// Clipping a triangle by three half spaces adds at most one corner per half space.
constexpr uint32_t max_corners = 6;
using Corners = FixedVector<glm::dvec2, max_corners>;
using Edge2d = radix::geometry::Edge<2, double>;
} // namespace

Corners clip_triangle(const Triangle2d &subject, const Triangle2d &clip) {
    const Triangle2d bounds = as_counter_clockwise(clip);
    const std::array<Edge2d, 3> edges = {Edge2d{bounds[0], bounds[1]}, Edge2d{bounds[1], bounds[2]}, Edge2d{bounds[2], bounds[0]}};

    Corners corners;
    corners.append_range(subject);
    return polygon::clip(corners, std::span<const Edge2d>(edges));
}

void clip_into_triangles(const Triangle2d &subject, const Triangle2d &clip, std::vector<Triangle2d> &out) {
    clip_into_triangles(subject, clip, [&out](const Triangle2d &piece) {
        out.push_back(piece);
    });
}

} // namespace polygon
