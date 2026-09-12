#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>
#include <meshoptimizer.h>

#include "glm_utils.h"
#include "range_utils.h"

namespace meshopt {

// The partition of each cluster, and how many partitions there are.
struct [[nodiscard]] PartitionClustersResult {
    std::vector<uint32_t> cluster_partitions;
    uint32_t partition_count = 0;
};

// Groups clusters into partitions of similar size, preferring clusters that share vertices or lie close together.
inline PartitionClustersResult partition_clusters(
    const std::span<const uint32_t> cluster_indices,       // flattened vertex indices of all clusters
    const std::span<const uint32_t> cluster_vertex_counts, // number of vertices per cluster
    const std::span<const glm::vec3> vertex_positions,
    const uint32_t clusters_per_partition) {
    ASSERT(clusters_per_partition > 0, "clusters_per_partition must be > 0");

    // Allocate cluster to partition map
    const size_t cluster_count = cluster_vertex_counts.size();
    std::vector<uint32_t> partitions(cluster_count);

    const size_t total_index_count = sum(cluster_vertex_counts, size_t{0});
    const std::span<const float> positions_flat = flatten(vertex_positions);

    const size_t partition_count = meshopt_partitionClusters(
        partitions.data(),
        cluster_indices.data(),
        total_index_count,
        cluster_vertex_counts.data(),
        cluster_count,
        positions_flat.data(),
        vertex_positions.size(),
        sizeof(glm::vec3),
        clusters_per_partition
    );

    return PartitionClustersResult{std::move(partitions), static_cast<uint32_t>(partition_count)};
}

} // namespace meshopt