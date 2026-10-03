#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "containers/FixedVector.h"
#include "geometry/geometry.h"
#include "polygon/clip_with.h"

namespace polygon {

using geometry::Triangle2d;

namespace detail {

// Clips 3D points against the side of a plane its normal points to.
struct PointPolicy3d {
    using Vertex = glm::dvec3;
    using HalfSpace = radix::geometry::Plane<double>;

    // Measures the signed distance of `point` from `plane`, positive on the side its normal points to.
    static double compute_distance(const HalfSpace &plane, const Vertex &point) {
        return radix::geometry::distance(plane, point);
    }

    // Computes the point where the edge from `from` to `to` crosses `plane`, at `t` along it.
    static Vertex compute_crossing(const HalfSpace & /*plane*/, const Vertex &from, const Vertex &to, const double t) {
        return geometry::lerp(from, to, t);
    }
};

// Clips 2D points against the left side of an edge.
struct PointPolicy2d {
    using Vertex = glm::dvec2;
    using HalfSpace = radix::geometry::Edge<2, double>;

    // Measures the orientation of `point` relative to `edge`, positive on its left.
    static double compute_distance(const HalfSpace &edge, const Vertex &point) {
        return geometry::orient(edge[0], edge[1], point);
    }

    // Computes the point where the edge from `from` to `to` crosses `edge`, at `t` along it.
    static Vertex compute_crossing(const HalfSpace & /*edge*/, const Vertex &from, const Vertex &to, const double t) {
        return geometry::lerp(from, to, t);
    }
};

} // namespace detail

// Clips the convex `polygon` to the side of `plane` its normal points to.
// Requires room for one more vertex than `polygon` has.
template <size_t capacity>
FixedVector<glm::dvec3, capacity> clip(const FixedVector<glm::dvec3, capacity> &polygon, const radix::geometry::Plane<double> &plane) {
    return clip_with<detail::PointPolicy3d>(polygon, plane);
}

// Clips the convex `polygon` to the side of every plane its normal points to.
// Requires room for one more vertex per plane.
template <size_t capacity>
FixedVector<glm::dvec3, capacity> clip(const FixedVector<glm::dvec3, capacity> &polygon, std::span<const radix::geometry::Plane<double>> planes) {
    FixedVector<glm::dvec3, capacity> result = polygon;
    for (const radix::geometry::Plane<double> &plane : planes) {
        result = clip(result, plane);
    }
    return result;
}

// Clips the convex `polygon` to the left of `edge`.
// Requires room for one more vertex than `polygon` has.
template <size_t capacity>
FixedVector<glm::dvec2, capacity> clip(const FixedVector<glm::dvec2, capacity> &polygon, const radix::geometry::Edge<2, double> &edge) {
    return clip_with<detail::PointPolicy2d>(polygon, edge);
}

// Clips the convex `polygon` to the left of every edge.
// Requires room for one more vertex per edge.
template <size_t capacity>
FixedVector<glm::dvec2, capacity> clip(const FixedVector<glm::dvec2, capacity> &polygon, std::span<const radix::geometry::Edge<2, double>> edges) {
    FixedVector<glm::dvec2, capacity> result = polygon;
    for (const radix::geometry::Edge<2, double> &edge : edges) {
        result = clip(result, edge);
    }
    return result;
}

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
