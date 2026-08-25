#pragma once

#include <cmath>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "geometry/geometry.h"

#include "bvh/layout.h"

namespace build {

// Extends the node's bounds by its error interval as a fourth axis, scaled by `error_weight`.
[[nodiscard]]
inline radix::geometry::Aabb<4, double> extend_by_error(const bvh::NodeHeader &header, const double error_weight) {
    // An unbounded parent error would swallow every other axis.
    const double parent_error = std::isinf(header.max_parent_error) ? header.min_child_error : header.max_parent_error;
    return {
        glm::dvec4(header.bounds.min, error_weight * header.min_child_error),
        glm::dvec4(header.bounds.max, error_weight * parent_error),
    };
}

// Computes the cost of putting both extended nodes under one parent: the surface area of what they merge into.
[[nodiscard]]
inline double compute_merge_cost(
    const radix::geometry::Aabb<4, double> &left, const radix::geometry::Aabb<4, double> &right) {
    radix::geometry::Aabb<4, double> merged = left;
    merged.expand_by(right);
    return geometry::compute_surface_area(merged);
}

// Computes the cost of putting both nodes under one parent: the surface area of what they
// merge into, with error as a fourth axis.
[[nodiscard]]
inline double compute_merge_cost(const bvh::NodeHeader &left, const bvh::NodeHeader &right, const double error_weight) {
    return compute_merge_cost(extend_by_error(left, error_weight), extend_by_error(right, error_weight));
}

} // namespace build
