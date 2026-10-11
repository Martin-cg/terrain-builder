#pragma once

#include <cmath>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "geometry/bounds.h"

#include "bvh/layout.h"

namespace build {

const double DEFAULT_ERROR_WEIGHT = 1.0;

// Extends the node's bounds by its error interval as a fourth axis, scaled by `error_weight`.
[[nodiscard]]
inline radix::geometry::Aabb<4, double> extend_by_error(const bvh::NodeHeader &header, const double error_weight = DEFAULT_ERROR_WEIGHT) {
    // An unbounded parent error would swallow every other axis.
    const double parent_error = std::isinf(header.max_parent_error) ? header.min_child_error : header.max_parent_error;
    return {
        glm::dvec4(header.bounds.min, error_weight * header.min_child_error),
        glm::dvec4(header.bounds.max, error_weight * parent_error),
    };
}

// Computes the cost of one node covering `bounds`.
[[nodiscard]]
inline double compute_node_cost(const radix::geometry::Aabb<4, double> &bounds) {
    return geometry::compute_surface_area(bounds);
}

// Computes the cost of keeping `header` as a node of its tree, with its error interval weighed in by `error_weight`.
[[nodiscard]]
inline double compute_node_cost(const bvh::NodeHeader &header, const double error_weight = DEFAULT_ERROR_WEIGHT) {
    return compute_node_cost(extend_by_error(header, error_weight));
}

// Computes the cost of putting both extended nodes under one parent.
[[nodiscard]]
inline double compute_merge_cost(
    const radix::geometry::Aabb<4, double> &left, const radix::geometry::Aabb<4, double> &right) {
    radix::geometry::Aabb<4, double> merged = left;
    merged.expand_by(right);
    return compute_node_cost(merged);
}

// Computes the cost of putting both nodes under one parent, with error as a fourth axis.
[[nodiscard]]
inline double compute_merge_cost(const bvh::NodeHeader &left, const bvh::NodeHeader &right, const double error_weight = DEFAULT_ERROR_WEIGHT) {
    return compute_merge_cost(extend_by_error(left, error_weight), extend_by_error(right, error_weight));
}

} // namespace build
