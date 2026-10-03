#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "containers/FixedVector.h"
#include "geometry/geometry.h"
#include "polygon/clip_with.h"

namespace polygon {

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

// Appends the overlap of two triangles as a fan of triangles without degenerate pieces.
// Appends nothing if they are disjoint, meet in at most a segment, or either is degenerate.
void clip_triangle(const geometry::Triangle2d &subject, const geometry::Triangle2d &clip, std::vector<geometry::Triangle2d> &out);

} // namespace polygon
