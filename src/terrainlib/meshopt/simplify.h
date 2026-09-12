#pragma once

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <meshoptimizer.h>

#include "glm_utils.h"

namespace meshopt {

inline constexpr size_t NO_TARGET_COUNT = 0;
inline constexpr float NO_TARGET_ERROR = FLT_MAX;

// The simplified triangles and the error meshopt reports for them.
struct [[nodiscard]] SimplifyResult {
    std::vector<glm::uvec3> triangles;
    float relative_error = 0.0f;
};

// Reduces the triangle count while preserving appearance, possibly stopping short of the target.
inline SimplifyResult simplify(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const size_t target_triangle_count,
    const float target_error,
    const uint32_t options = 0) {
    const size_t vertex_count = positions.size();
    const size_t triangle_count = triangles.size();
    const size_t index_count = triangle_count * 3;
    const size_t positions_stride = sizeof(glm::vec3);
    const size_t target_index_count = target_triangle_count * 3;

    SimplifyResult result;
    result.triangles.resize(triangle_count);

    // Flatten spans
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);
    const std::span<uint32_t> result_indices = flatten(result.triangles);

    const size_t new_index_count = meshopt_simplify(
        result_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        vertex_count,
        positions_stride,
        target_index_count,
        target_error,
        options,
        &result.relative_error);

    // Resize to match returned number of indices
    const size_t new_triangle_count = new_index_count / 3;
    result.triangles.resize(new_triangle_count);

    return result;
}

// Reduces the triangle count while preserving appearance and vertex attributes, possibly stopping short of the target.
inline SimplifyResult simplify_with_attributes(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const std::span<const float> vertex_attributes,
    const size_t vertex_attribute_stride,
    const std::span<const float> vertex_attribute_weights,
    const std::span<const uint8_t> vertex_locks,
    const size_t target_triangle_count,
    const float target_error,
    const uint32_t options) {
    const size_t vertex_count = positions.size();
    const size_t triangle_count = triangles.size();
    const size_t index_count = triangle_count * 3;
    const size_t attribute_count = vertex_attribute_weights.size();
    const size_t positions_stride = sizeof(glm::vec3);
    const size_t target_index_count = target_triangle_count * 3;

    DEBUG_ASSERT(vertex_attributes.size() >= vertex_count * (vertex_attribute_stride / sizeof(float)));
    DEBUG_ASSERT(vertex_locks.size() == vertex_count || vertex_locks.empty());

    SimplifyResult result;
    result.triangles.resize(triangle_count);

    // Flatten spans
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);
    const std::span<uint32_t> result_indices = flatten(result.triangles);
    const uint8_t* vertex_lock_ptr = vertex_locks.empty() ? nullptr : vertex_locks.data();

    const size_t new_index_count = meshopt_simplifyWithAttributes(
        result_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        vertex_count,
        positions_stride,
        vertex_attributes.data(),
        vertex_attribute_stride,
        vertex_attribute_weights.data(),
        attribute_count,
        vertex_lock_ptr,
        target_index_count,
        target_error,
        options,
        &result.relative_error);

    // Resize to match returned number of indices
    const size_t new_triangle_count = new_index_count / 3;
    result.triangles.resize(new_triangle_count);

    return result;
}

} // namespace meshopt
