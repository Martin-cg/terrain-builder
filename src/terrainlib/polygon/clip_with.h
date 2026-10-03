#pragma once

#include <concepts>
#include <cstddef>

#include "containers/FixedVector.h"

namespace polygon {

// A vertex type and the half space it is clipped against.
template <typename Policy>
concept ClipPolicy = requires(
    const typename Policy::HalfSpace &half_space,
    const typename Policy::Vertex &vertex,
    const typename Policy::Vertex &from,
    const typename Policy::Vertex &to,
    const double t) {
    // Measures the signed distance of `vertex` from `half_space`, positive on the side that is kept.
    { Policy::compute_distance(half_space, vertex) } -> std::same_as<double>;
    // Computes the vertex where the edge from `from` to `to` crosses `half_space`, at `t` along it.
    { Policy::compute_crossing(half_space, from, to, t) } -> std::same_as<typename Policy::Vertex>;
};

// Clips the convex `polygon` to `half_space`, for vertices that carry more than a position.
// A new vertex is the same whichever way its edge is walked.
template <ClipPolicy Policy, size_t capacity>
FixedVector<typename Policy::Vertex, capacity> clip_with(
    const FixedVector<typename Policy::Vertex, capacity> &polygon,
    const typename Policy::HalfSpace &half_space) {
    using Vertex = typename Policy::Vertex;

    FixedVector<Vertex, capacity> result;
    if (polygon.empty()) {
        return result;
    }

    // Measuring t from the inside vertex makes a new vertex independent of the direction its edge is walked.
    const Vertex *previous = &polygon.back();
    double previous_distance = Policy::compute_distance(half_space, *previous);
    for (const Vertex &current : polygon) {
        const double current_distance = Policy::compute_distance(half_space, current);
        if (previous_distance > 0 && current_distance < 0) {
            const double t = previous_distance / (previous_distance - current_distance);
            result.push_back(Policy::compute_crossing(half_space, *previous, current, t));
        } else if (previous_distance < 0 && current_distance > 0) {
            const double t = current_distance / (current_distance - previous_distance);
            result.push_back(Policy::compute_crossing(half_space, current, *previous, t));
        }
        if (current_distance >= 0) {
            result.push_back(current);
        }

        previous = &current;
        previous_distance = current_distance;
    }
    return result;
}

} // namespace polygon
