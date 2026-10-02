#pragma once

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <ranges>
#include <tuple>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "cluster.h"
#include "containers/OffsetTable.h"
#include "containers/SegmentedBuffer.h"
#include "containers/StampSet.h"
#include "enumerate.h"
#include "mesh/connectivity/edges.h"
#include "mesh/normalize.h"
#include "numeric/int_math.h"
#include "range_utils.h"

// Per cluster, the clusters sharing a vertex with it.
using ClusterAdjacency = SegmentedBuffer<uint32_t, uint32_t>;

// An edge a cluster shares with a neighbouring cluster.
struct SharedClusterEdge {
    uint32_t cluster;
    uint32_t neighbour;
    glm::uvec2 edge;

    bool operator==(const SharedClusterEdge &) const = default;
};

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

// A half-edge of a cluster, in global vertex indices.
struct ClusterHalfedge {
    uint32_t cluster;
    glm::uvec2 edge;

    bool operator==(const ClusterHalfedge &) const = default;
};

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

// Collects the half-edges of every cluster whose ends are both used by more than one cluster.
// Normalizes them if `normalize` is set.
inline std::vector<ClusterHalfedge> collect_halfedges_between_shared_vertices(const Clustering &clustering, const bool normalize) {
    // Every cluster sharing an edge uses both its ends.
    const std::vector<uint8_t> cluster_counts = count_vertex_adjacent_clusters<uint8_t>(clustering);
    const auto is_shared = [&](const uint32_t vertex) {
        return cluster_counts[vertex] >= 2;
    };

    std::vector<ClusterHalfedge> halfedges;
    for (const auto [cluster_index, cluster] : enumerate<uint32_t>(clustering.clusters)) {
        mesh::for_each_halfedge(cluster.local_triangles, [&](const glm::uvec2 local_edge) {
            const glm::uvec2 edge(cluster.vertex_indices[local_edge.x], cluster.vertex_indices[local_edge.y]);
            if (!is_shared(edge.x) || !is_shared(edge.y)) {
                return;
            }
            halfedges.push_back({cluster_index, normalize ? mesh::normalize_edge(edge) : edge});
        });
    }
    return halfedges;
}

// Pairs every two clusters holding the same edge, in both orders.
// Expects `halfedges` normalized if `normalized` is set.
inline std::vector<SharedClusterEdge> pair_clusters_by_edge(std::vector<ClusterHalfedge> halfedges, const bool normalized) {
    DEBUG_ASSERT(!normalized || std::ranges::all_of(halfedges, [](const ClusterHalfedge &h) {
        return h.edge == mesh::normalize_edge(h.edge);
    }));

    // The edge regardless of winding, which a stored edge already is when normalized.
    const auto edge_key = [normalized](const ClusterHalfedge &h) {
        return normalized ? h.edge : mesh::normalize_edge(h.edge);
    };
    std::ranges::sort(halfedges, {}, [&](const ClusterHalfedge &h) {
        const glm::uvec2 key = edge_key(h);
        return std::tuple(key.x, key.y, h.cluster);
    });

    const auto same_edge = [&](const ClusterHalfedge &first, const ClusterHalfedge &second) {
        return edge_key(first) == edge_key(second);
    };
    const auto same_cluster = [](const ClusterHalfedge &first, const ClusterHalfedge &second) {
        return first.cluster == second.cluster;
    };
    // A cluster can hold an edge through several of its triangles. It shares the edge once, in their winding if they agree, else normalized.
    const auto merge_windings = [&](const auto windings) {
        const ClusterHalfedge &first = windings.front();
        const bool windings_agree = std::ranges::all_of(windings, [&](const ClusterHalfedge &h) {
            return h.edge == first.edge;
        });
        return ClusterHalfedge{first.cluster, windings_agree ? first.edge : edge_key(first)};
    };

    std::vector<SharedClusterEdge> shared_edges;
    for (const auto edge_halfedges : halfedges | std::views::chunk_by(same_edge)) {
        auto holders = edge_halfedges | std::views::chunk_by(same_cluster) | std::views::transform(merge_windings);
        for (const ClusterHalfedge holder : holders) {
            for (const ClusterHalfedge other : holders) {
                if (other.cluster != holder.cluster) {
                    shared_edges.push_back({holder.cluster, other.cluster, holder.edge});
                }
            }
        }
    }
    return shared_edges;
}

// Groups `shared_edges` into one segment per cluster in [0, cluster_count), each ordered by neighbour, then edge.
inline SegmentedBuffer<SharedClusterEdge, uint32_t> group_shared_edges_by_cluster(const uint32_t cluster_count, std::vector<SharedClusterEdge> shared_edges) {
    std::ranges::sort(shared_edges, {}, [](const SharedClusterEdge &e) {
        return std::tuple(e.cluster, e.neighbour, e.edge.x, e.edge.y);
    });

    std::vector<uint32_t> segment_sizes(cluster_count, 0);
    for (const SharedClusterEdge &shared_edge : shared_edges) {
        segment_sizes[shared_edge.cluster]++;
    }
    OffsetTable_<uint32_t> offsets;
    offsets.append_lengths(segment_sizes);

    return SegmentedBuffer<SharedClusterEdge, uint32_t>(std::move(shared_edges), std::move(offsets));
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

// Finds, per cluster, the edges it shares with other clusters, ordered by neighbour, then edge.
// Each edge is wound as in the cluster's triangles, or normalized if `normalize` is set or the cluster holds it both ways.
[[nodiscard]]
inline SegmentedBuffer<SharedClusterEdge, uint32_t> find_shared_cluster_edges(const Clustering &clustering, const bool normalize = false) {
    std::vector<detail::ClusterHalfedge> halfedges = detail::collect_halfedges_between_shared_vertices(clustering, normalize);
    std::vector<SharedClusterEdge> shared_edges = detail::pair_clusters_by_edge(std::move(halfedges), normalize);
    return detail::group_shared_edges_by_cluster(clustering.cluster_count(), std::move(shared_edges));
}
