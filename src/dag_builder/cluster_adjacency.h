#pragma once

#include <concepts>
#include <cstdint>
#include <vector>

#include "cluster.h"
#include "containers/SegmentedBuffer.h"
#include "containers/StampSet.h"
#include "enumerate.h"
#include "numeric/int_math.h"
#include "range_utils.h"

// Per cluster, the clusters sharing a vertex with it.
using ClusterAdjacency = SegmentedBuffer<uint32_t, uint32_t>;

// Counts, per vertex, the clusters using it, saturating at the largest Count.
// Expects each cluster to list a vertex at most once.
template <std::unsigned_integral Count = uint32_t>
[[nodiscard]]
std::vector<Count> count_vertex_adjacent_clusters(const Clustering &clustering) {
    std::vector<Count> counts(clustering.vertex_count(), 0);
    for (const Cluster &cluster : clustering.clusters) {
        for (const uint32_t vertex : cluster.vertex_indices) {
            counts[vertex] = saturating_inc(counts[vertex]);
        }
    }
    return counts;
}

namespace detail {

// Per vertex, the clusters using it.
inline SegmentedBuffer<uint32_t, uint32_t> invert_cluster_vertices(const Clustering &clustering) {
    std::vector<uint32_t> cluster_counts = count_vertex_adjacent_clusters(clustering);

    SegmentedBuffer<uint32_t, uint32_t> clusters_of_vertex;
    clusters_of_vertex.init(cluster_counts);

    // The counts double as write cursors, walked back down as each cluster takes its slot.
    for (const auto [cluster_index, cluster] : enumerate<uint32_t>(clustering.clusters)) {
        for (const uint32_t vertex : cluster.vertex_indices) {
            cluster_counts[vertex]--;
            clusters_of_vertex.segment(vertex)[cluster_counts[vertex]] = cluster_index;
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
