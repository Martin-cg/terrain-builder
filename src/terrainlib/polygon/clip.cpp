#include <array>
#include <cstdint>
#include <span>

#include "containers/FixedVector.h"
#include "mesh/geometry.h"
#include "mesh/WindingOrder.h"
#include "polygon/clip.h"
#include "polygon/triangulate.h"

namespace polygon {

namespace {
// Clipping a triangle by three half spaces adds at most one corner per half space.
constexpr uint32_t max_corners = 6;
using Corners = FixedVector<glm::dvec2, max_corners>;
using Edge2d = radix::geometry::Edge<2, double>;
} // namespace

void clip_triangle(const geometry::Triangle2d &subject, const geometry::Triangle2d &clip, std::vector<geometry::Triangle2d> &out) {
    const geometry::Triangle2d bounds = as_counter_clockwise(clip);
    const std::array<Edge2d, 3> edges = {Edge2d{bounds[0], bounds[1]}, Edge2d{bounds[1], bounds[2]}, Edge2d{bounds[2], bounds[0]}};

    Corners corners;
    corners.append_range(subject);
    const Corners clipped = polygon::clip(corners, std::span<const Edge2d>(edges));

    // Corners on a clip edge are kept by both sides, so the fan can hold collinear runs.
    triangulate_fan(clipped, [&](const geometry::Triangle2d &triangle) {
        if (!geometry::is_empty_triangle(triangle)) {
            out.push_back(triangle);
        }
    });
}

} // namespace polygon
