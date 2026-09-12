#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <meshoptimizer.h>

#include "glm_utils.h"

namespace meshopt {

// Removes degenerate and same-winding duplicate triangles, comparing vertices by their bytes in `vertices`.
template <std::ranges::contiguous_range Vertices>
[[nodiscard]] inline std::vector<glm::uvec3> filter_index_buffer(const std::span<const glm::uvec3> triangles, const Vertices &vertices) {
    using Vertex = std::ranges::range_value_t<Vertices>;
    static_assert(std::is_trivially_copyable_v<Vertex>, "Vertex type must be trivially copyable");
    static_assert(sizeof(Vertex) <= 256, "meshopt compares at most 256 bytes per vertex");

    const size_t vertex_count = std::ranges::size(vertices);
    const std::span<const uint32_t> indices = flatten(triangles);
    DEBUG_ASSERT(std::ranges::all_of(indices, [vertex_count](const uint32_t index) { return index < vertex_count; }));

    std::vector<glm::uvec3> filtered(triangles.size());
    const size_t filtered_index_count = meshopt_filterIndexBuffer(
        flatten(filtered).data(),
        indices.data(),
        indices.size(),
        std::ranges::data(vertices),
        vertex_count,
        sizeof(Vertex),
        sizeof(Vertex));

    const size_t filtered_triangle_count = filtered_index_count / 3;
    filtered.resize(filtered_triangle_count);
    return filtered;
}

} // namespace meshopt