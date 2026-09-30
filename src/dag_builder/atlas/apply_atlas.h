#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "cluster.h"
#include "range_utils.h"
#include "uv/atlas.h"

// Adopt the atlas layout, duplicating the cluster's seam vertices.
[[nodiscard]]
inline Cluster apply_atlas(const Cluster &cluster, uv::Atlas atlas) {
    Cluster applied = cluster;

    applied.vertex_indices = transform_vector(atlas.vertex_map, [&](const uint32_t local) {
        return cluster.vertex_indices[local];
    });
    applied.local_triangles = std::move(atlas.triangles);
    applied.uvs = std::move(atlas.uvs);
    return applied;
}

namespace detail {
constexpr uint32_t no_atlas_vertex = std::numeric_limits<uint32_t>::max();

// Rebuild one cluster from its slice of a node atlas, renumbering the atlas vertices it
// touches. A seam duplicates a vertex, so two atlas vertices can map to one clustering vertex.
[[nodiscard]]
inline Cluster take_atlas_slice(
    const Cluster &cluster,
    const uv::Atlas &atlas,
    const uint32_t triangle_offset,
    const std::span<uint32_t> vertex_remap /* scratch buffer */) {
    Cluster applied = cluster;
    applied.vertex_indices.clear();
    applied.local_triangles.clear();
    applied.uvs.clear();

    for (const uint32_t i : range(cluster.triangle_count())) {
        const glm::uvec3 &atlas_triangle = atlas.triangles[triangle_offset + i];
        glm::uvec3 local;
        for (const uint8_t corner : range<uint8_t>(3)) {
            const uint32_t atlas_vertex = atlas_triangle[corner];
            uint32_t &local_vertex = vertex_remap[atlas_vertex];
            if (local_vertex == no_atlas_vertex) {
                local_vertex = applied.vertex_indices.size();
                applied.vertex_indices.push_back(atlas.vertex_map[atlas_vertex]);
                applied.uvs.push_back(atlas.uvs[atlas_vertex]);
            }
            local[corner] = local_vertex;
        }
        applied.local_triangles.push_back(local);
    }

    for (const uint32_t i : range(cluster.triangle_count())) {
        const glm::uvec3 &atlas_triangle = atlas.triangles[triangle_offset + i];
        for (const uint8_t corner : range<uint8_t>(3)) {
            vertex_remap[atlas_triangle[corner]] = no_atlas_vertex;
        }
    }
    return applied;
}
} // namespace detail

// Give each cluster the uvs a node atlas laid out for it. Clusters must arrive in the order
// their triangles were handed to the unwrap, since that is how the atlas is sliced.
inline void apply_node_atlas(
    Clustering &clustering,
    const std::span<const uint32_t> cluster_indices,
    const uv::Atlas &atlas) {
    std::vector<uint32_t> vertex_remap(atlas.uvs.size(), detail::no_atlas_vertex);

    uint32_t triangle_offset = 0;
    for (const uint32_t cluster_index : cluster_indices) {
        Cluster &cluster = clustering.clusters[cluster_index];
        const uint32_t triangle_count = cluster.triangle_count();
        cluster = detail::take_atlas_slice(cluster, atlas, triangle_offset, vertex_remap);
        triangle_offset += triangle_count;
    }
    DEBUG_ASSERT(triangle_offset == atlas.triangles.size());
}
