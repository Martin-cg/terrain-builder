#pragma once

#include <array>
#include <cstdint>
#include <ranges>
#include <span>
#include <vector>

#include <libassert/assert.hpp>
#include <metis.h>

#include "Partitioning.h"
#include "cluster.h"
#include "cluster_adjacency.h"
#include "numeric/int_math.h"
#include "partitioning/Provider.h"
#include "range_utils.h"

namespace partitioning::provider {

namespace detail {

// A graph in the compressed adjacency layout METIS reads, with a weight per edge.
struct MetisGraph {
    std::vector<idx_t> xadj;
    std::vector<idx_t> adjncy;
    std::vector<idx_t> adjwgt;
};

// Builds the graph of clusters sharing an edge, weighted by how many edges they share.
inline MetisGraph build_shared_edge_graph(const Clustering &clustering) {
    const SegmentedBuffer<SharedClusterEdge, uint32_t> shared_edges = find_shared_cluster_edges(clustering, /* normalize */ true);
    const auto same_neighbour = [](const SharedClusterEdge &first, const SharedClusterEdge &second) {
        return first.neighbour == second.neighbour;
    };

    MetisGraph graph;
    graph.xadj.push_back(0);
    for (const std::span<const SharedClusterEdge> cluster_edges : shared_edges.segments()) {
        for (const auto neighbour_edges : cluster_edges | std::views::chunk_by(same_neighbour)) {
            graph.adjncy.push_back(static_cast<idx_t>(neighbour_edges.front().neighbour));
            graph.adjwgt.push_back(static_cast<idx_t>(std::ranges::distance(neighbour_edges)));
        }
        graph.xadj.push_back(static_cast<idx_t>(graph.adjncy.size()));
    }
    return graph;
}

// Splits `graph` into `partition_count` parts, keeping the weight of the edges between them low.
inline std::vector<idx_t> partition_graph(MetisGraph &graph, idx_t partition_count) {
    std::array<idx_t, METIS_NOPTIONS> options = {};
    METIS_SetDefaultOptions(options.data());
    options[METIS_OPTION_NUMBERING] = 0;
    options[METIS_OPTION_OBJTYPE] = METIS_OBJTYPE_CUT;

    idx_t vertex_count = static_cast<idx_t>(graph.xadj.size()) - 1;
    idx_t constraint_count = 1;
    idx_t edge_cut = 0;
    std::vector<idx_t> vertex_partitions(vertex_count, 0);

    const int result = METIS_PartGraphKway(
        &vertex_count,
        &constraint_count,
        graph.xadj.data(),
        graph.adjncy.data(),
        nullptr,
        nullptr,
        graph.adjwgt.data(),
        &partition_count,
        nullptr,
        nullptr,
        options.data(),
        &edge_cut,
        vertex_partitions.data());
    ASSERT(METIS_OK == result);

    return vertex_partitions;
}

} // namespace detail

// Groups clusters into partitions of about the target size, keeping the edges shared between partitions few.
class Metis : public Provider {
public:
    Partitioning partition(const Clustering &clustering, const uint32_t clusters_per_partition) const override {
        const uint32_t cluster_count = clustering.clusters.size();
        const uint32_t partition_count = int_div_ceil(cluster_count, clusters_per_partition);

        // METIS expects more than one part, so clusters too few to split all go into one partition.
        if (partition_count < 2) {
            return Partitioning{1, clusters_per_partition, std::vector<uint32_t>(cluster_count, 0u)};
        }

        detail::MetisGraph graph = detail::build_shared_edge_graph(clustering);
        const std::vector<idx_t> graph_partitions = detail::partition_graph(graph, static_cast<idx_t>(partition_count));
        std::vector<uint32_t> cluster_partitions = transform_vector(graph_partitions, [](const idx_t partition) {
            return static_cast<uint32_t>(partition);
        });

        return Partitioning{partition_count, clusters_per_partition, std::move(cluster_partitions)};
    }
};

} // namespace partitioning::provider
