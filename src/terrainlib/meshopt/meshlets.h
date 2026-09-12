#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <meshoptimizer.h>

#include "glm_utils.h"

namespace meshopt {

// The most vertices a meshlet can hold.
inline constexpr size_t MAX_MESHLET_VERTICES = 256;

// Meshlets over a shared vertex buffer, each meshlet's data following the previous one's.
struct [[nodiscard]] BuildMeshletsResult {
    std::vector<meshopt_Meshlet> meshlets; // offsets into vertex_indices and local_triangles, counted in elements of each
    std::vector<uint32_t> vertex_indices;  // flat vertex indices for all meshlets
    std::vector<glm::tvec3<uint8_t>> local_triangles; // flat triangle indices for all meshlets
};

// Splits a mesh into meshlets.
inline BuildMeshletsResult build_meshlets(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec3> positions,
    const uint32_t max_vertices,
    const uint32_t min_triangles,
    const uint32_t max_triangles,
    const float cone_weight = 0.5f,
    const float split_factor = 0.0f) {
    ASSERT(max_vertices <= MAX_MESHLET_VERTICES);

    // Flatten inputs
    const std::span<const uint32_t> indices = flatten(triangles);
    const std::span<const float> positions_flat = flatten(positions);

    // Compute maximum number of meshlets needed
    const size_t index_count = indices.size();
    const uint32_t max_meshlets = meshopt_buildMeshletsBound(index_count, max_vertices, min_triangles);

    // Allocate buffers
    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<uint32_t> vertex_indices(index_count);
    std::vector<glm::tvec3<uint8_t>> local_triangles(triangles.size());
    std::span<uint8_t> local_indices = flatten(local_triangles);

    // Build meshlets
    const size_t meshlet_count = meshopt_buildMeshletsFlex(
        meshlets.data(),
        vertex_indices.data(),
        local_indices.data(),
        indices.data(),
        index_count,
        positions_flat.data(),
        positions.size(),
        sizeof(glm::vec3),
        max_vertices,
        min_triangles,
        max_triangles,
        cone_weight,
        split_factor);
    ASSERT(meshlet_count <= max_meshlets);

    // Trim meshlet output buffer
    meshlets.resize(meshlet_count);

    // Convert index offsets into triangle offsets
    for (meshopt_Meshlet& meshlet : meshlets) {
        DEBUG_ASSERT(meshlet.triangle_offset % 3 == 0);
        meshlet.triangle_offset /= 3;
    }

    // Trim unused vertex and triangle buffers
    if (meshlet_count > 0) {
        const meshopt_Meshlet &last = meshlets[meshlet_count - 1];
        const size_t full_vertex_count = last.vertex_offset + last.vertex_count;
        const size_t full_triangle_count = last.triangle_offset + last.triangle_count;
        ASSERT(vertex_indices.size() >= full_vertex_count);
        ASSERT(local_triangles.size() >= full_triangle_count);
        vertex_indices.resize(full_vertex_count);
        local_triangles.resize(full_triangle_count);
    }

    return BuildMeshletsResult{
        std::move(meshlets),
        std::move(vertex_indices),
        std::move(local_triangles)};
}

// Reorders a meshlet for locality.
// Every vertex in `vertex_indices` must be used by a triangle.
inline void optimize_meshlet(
    const std::span<uint32_t> vertex_indices,
    const std::span<glm::tvec3<uint8_t>> local_triangles) {
    const std::span<uint8_t> local_indices = flatten(local_triangles);

    meshopt_optimizeMeshlet(
        vertex_indices.data(),
        local_indices.data(),
        local_triangles.size(),
        vertex_indices.size());
}

} // namespace meshopt
