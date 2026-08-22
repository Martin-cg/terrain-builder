#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>

#include <libassert/assert.hpp>

#include "geometry/geometry.h"

#include "bvh/layout.h"

namespace bvh {

// Aggregate metrics from multiple headers into a parent header.
template <std::ranges::input_range R>
NodeHeader merge_headers(R &&headers) {
    auto it = std::ranges::begin(headers);
    const auto end = std::ranges::end(headers);

    DEBUG_ASSERT(it != end);

    NodeHeader result = *it;
    it++;

    for (; it != end; it++) {
        const NodeHeader &child = *it;
        result.min_child_error = std::min(result.min_child_error, child.min_child_error);
        result.max_parent_error = std::max(result.max_parent_error, child.max_parent_error);
        result.triangle_count += child.triangle_count;
        result.bounds = geometry::union_bounds(result.bounds, child.bounds);
    }

    return result;
}

} // namespace bvh
