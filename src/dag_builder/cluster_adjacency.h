#pragma once

#include <cstdint>
#include <vector>

#include "cluster.h"
#include "containers/SegmentedBuffer.h"
#include "containers/StampSet.h"
#include "enumerate.h"
#include "range_utils.h"

// Per cluster, the clusters sharing a vertex with it.
using ClusterAdjacency = SegmentedBuffer<uint32_t, uint32_t>;

namespace detail {

// Per vertex, the clusters using it.
inline SegmentedBuffer<uint32_t, uint32_t> invert_cluster_vertices(const Clustering &clustering) {
    std::vector<uint32_t> per_vertex(clustering.vertex_count(), 0);
    for (const Cluster &cluster : clustering.clusters) {
        for (const uint32_t vertex : cluster.vertex_indices) {
            per_vertex[vertex]++;
        }
    }

    SegmentedBuffer<uint32_t, uint32_t> clusters_of_vertex;
    clusters_of_vertex.init(per_vertex);

    // The counts double as write cursors, walked back down as each cluster takes its slot.
    for (const auto [cluster_index, cluster] : enumerate<uint32_t>(clustering.clusters)) {
        for (const uint32_t vertex : cluster.vertex_indices) {
            clusters_of_vertex.segment(vertex)[--per_vertex[vertex]] = cluster_index;
        }
    }

    return clusters_of_vertex;
}

} // namespace detail

// Gathers, for every cluster, the clusters it shares a vertex with. It is not its own neighbour.
[[nodiscard]]
inline ClusterAdjacency compute_cluster_adjacency(const Clustering &clustering) {
    const SegmentedBuffer<uint32_t, uint32_t> clusters_of_vertex = detail::invert_cluster_vertices(clustering);

    ClusterAdjacency adjacency;
    StampSet visited(clustering.cluster_count());

    for (const auto [cluster_index, cluster] : enumerate<uint32_t>(clustering.clusters)) {
        adjacency.start_new_segment();
        visited.reset(clustering.cluster_count());
        visited.insert(cluster_index);

        for (const uint32_t vertex : cluster.vertex_indices) {
            for (const uint32_t neighbour : clusters_of_vertex.segment(vertex)) {
                if (visited.insert(neighbour)) {
                    adjacency.push_to_last_segment(neighbour);
                }
            }
        }
    }

    return adjacency;
}
