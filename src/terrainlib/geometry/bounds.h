#pragma once

#include <limits>

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <radix/geometry.h>

namespace geometry {

// Grow bounds by the same distance on every side.
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_absolute(const radix::geometry::Aabb<n_dims, T> &bounds, const T padding) {
    using Vec = glm::vec<n_dims, T>;
    return radix::geometry::Aabb<n_dims, T>(bounds.min - Vec(padding), bounds.max + Vec(padding));
}

// Grow bounds by the given fraction of their size on every side.
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_relative(const radix::geometry::Aabb<n_dims, T> &bounds, const T fraction, const T min_padding = T(0)) {
    using Vec = glm::vec<n_dims, T>;
    const Vec padding = glm::max(bounds.size() * fraction, Vec(min_padding));
    return radix::geometry::Aabb<n_dims, T>(bounds.min - padding, bounds.max + padding);
}

// Map `point` to [0, 1] within `bounds`, clamped.
template <glm::length_t n_dims, typename T>
glm::vec<n_dims, T> relativize_point(const radix::geometry::Aabb<n_dims, T> &bounds, const glm::vec<n_dims, T> &point) {
    using Vec = glm::vec<n_dims, T>;
    const Vec size = glm::max(bounds.size(), Vec(std::numeric_limits<T>::min()));
    return glm::clamp((point - bounds.min) / size, T(0), T(1));
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> union_bounds(const radix::geometry::Aabb<n_dims, T> &a, const radix::geometry::Aabb<n_dims, T> &b) {
    radix::geometry::Aabb<n_dims, T> bounds = a;
    bounds.expand_by(b);
    return bounds;
}

template <glm::length_t n_dims, typename T>
T compute_surface_area(const radix::geometry::Aabb<n_dims, T> &bounds) {
    const glm::vec<n_dims, T> size = bounds.size();
    return glm::compAdd(size) * glm::compAdd(size) - glm::compAdd(size * size);
}

// The mean of the extents, as a single measure of how large the bounds are.
template <glm::length_t n_dims, typename T>
T compute_mean_extent(const radix::geometry::Aabb<n_dims, T> &bounds) {
    return glm::compAdd(bounds.size()) / static_cast<T>(n_dims);
}

} // namespace geometry
