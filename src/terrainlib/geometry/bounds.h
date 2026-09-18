#pragma once

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "VecRange.h"

namespace geometry {

// The smallest box containing every point.
template <AnyVecRange Range>
radix::geometry::Aabb<range_dims_v<Range>, range_scalar_t<Range>> calculate_bounds(const Range &points);

// Grow bounds by the same distance on every side.
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_absolute(const radix::geometry::Aabb<n_dims, T> &bounds, const T padding);

// Grow bounds by the given fraction of their size on every side.
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_relative(const radix::geometry::Aabb<n_dims, T> &bounds, const T fraction, const T min_padding = T(0));

}

#include "bounds.inl"
