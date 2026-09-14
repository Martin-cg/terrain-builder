#pragma once

#include <concepts>
#include <cstdint>
#include <vector>

#include <radix/geometry.h>

#include "containers/FixedVector.h"
#include "geometry/geometry.h"

namespace polygon {

using geometry::Triangle2d;

// Clips the subject triangle against the clip triangle and returns the intersection.
[[nodiscard]]
FixedVector<glm::dvec2, 6> clip_triangle(const Triangle2d &subject, const Triangle2d &clip);

// Clips the subject triangle against the clip triangle and fans the result into triangles.
template <std::invocable<const Triangle2d &> Visitor>
void clip_into_triangles(const Triangle2d &subject, const Triangle2d &clip, Visitor &&visit) {
    const FixedVector<glm::dvec2, 6> corners = clip_triangle(subject, clip);

    // Corners on a clip edge are kept by both sides, so the fan can hold collinear runs.
    for (uint32_t corner = 2; corner < corners.size(); corner++) {
        const Triangle2d piece = {corners[0], corners[corner - 1], corners[corner]};
        if (!geometry::is_empty_triangle(piece)) {
            visit(piece);
        }
    }
}

// Clips the subject triangle against the clip triangle and fans the result into triangles.
void clip_into_triangles(const Triangle2d &subject, const Triangle2d &clip, std::vector<Triangle2d> &out);

} // namespace polygon
