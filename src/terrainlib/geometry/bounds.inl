#include <limits>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "VecRange.h"

namespace geometry {

template <AnyVecRange Range>
radix::geometry::Aabb<range_dims_v<Range>, range_scalar_t<Range>> calculate_bounds(const Range &points) {
    using T = range_scalar_t<Range>;
    constexpr glm::length_t n_dims = range_dims_v<Range>;
    constexpr T inf = std::numeric_limits<T>::infinity();

    radix::geometry::Aabb<n_dims, T> bounds;
    bounds.min = glm::vec<n_dims, T>(+inf);
    bounds.max = glm::vec<n_dims, T>(-inf);
    for (const auto &point : points) {
        bounds.expand_by(point);
    }
    return bounds;
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_absolute(const radix::geometry::Aabb<n_dims, T> &bounds, const T padding) {
    using Vec = glm::vec<n_dims, T>;
    return radix::geometry::Aabb<n_dims, T>(bounds.min - Vec(padding), bounds.max + Vec(padding));
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_relative(const radix::geometry::Aabb<n_dims, T> &bounds, const T fraction, const T min_padding) {
    using Vec = glm::vec<n_dims, T>;
    const Vec padding = glm::max(bounds.size() * fraction, Vec(min_padding));
    return radix::geometry::Aabb<n_dims, T>(bounds.min - padding, bounds.max + padding);
}

}
