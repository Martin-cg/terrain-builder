#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "Partitioning.h"
#include "cluster.h"
#include "geometry/geometry.h"
#include "meshopt.h"
#include "partitioning/Provider.h"

namespace partitioning::provider {

// Groups clusters into partitions of about the target size, preferring clusters that share vertices or lie close together.
class Meshopt : public Provider {
public:
    Partitioning partition(const Clustering &clustering, const uint32_t clusters_per_partition) const override {
        const uint32_t cluster_count = clustering.clusters.size();

        // meshopt only supports float positions
        const std::vector<glm::vec3> positions_f = geometry::to_approximate_normalized(clustering.positions);

        // Prepare vertex counts per cluster and total vertex count
        std::vector<uint32_t> cluster_vertex_counts(cluster_count);
        uint32_t total_index_count = 0;
        for (uint32_t i = 0; i < cluster_count; i++) {
            cluster_vertex_counts[i] = clustering.clusters[i].vertex_indices.size();
            total_index_count += cluster_vertex_counts[i];
        }

        // Flatten vertices into index buffer
        std::vector<uint32_t> cluster_vertices(total_index_count);
        uint32_t offset = 0;
        for (uint32_t i = 0; i < cluster_count; i++) {
            const auto &vertices = clustering.clusters[i].vertex_indices;
            std::copy(vertices.begin(), vertices.end(), cluster_vertices.begin() + offset);
            offset += vertices.size();
        }

        meshopt::PartitionClustersResult partition_result = meshopt::partition_clusters(
            cluster_vertices,
            cluster_vertex_counts,
            positions_f,
            clusters_per_partition);

        return Partitioning{
            partition_result.partition_count,
            clusters_per_partition,
            std::move(partition_result.cluster_partitions)};
    }
};

} // namespace partitioning::provider
