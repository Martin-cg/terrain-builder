#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "cluster.h"
#include "cluster_adjacency.h"
#include "range_utils.h"

namespace {

Clustering make_clustering(
    const std::vector<glm::dvec3> &positions,
    const std::vector<std::vector<glm::uvec3>> &clusters) {
    Clustering clustering;
    clustering.positions = positions;

    for (const std::vector<glm::uvec3> &triangles : clusters) {
        Cluster cluster;
        cluster.id = clustering.clusters.size();

        std::unordered_map<uint32_t, uint32_t> local_of_global;
        const auto take = [&](const uint32_t global) {
            const auto [entry, inserted] = local_of_global.emplace(global, cluster.vertex_count());
            if (inserted) {
                cluster.vertex_indices.push_back(global);
            }
            return entry->second;
        };

        for (const glm::uvec3 &triangle : triangles) {
            cluster.local_triangles.emplace_back(take(triangle.x), take(triangle.y), take(triangle.z));
        }
        clustering.clusters.push_back(std::move(cluster));
    }

    return clustering;
}

bool are_neighbours(const ClusterAdjacency &adjacency, const uint32_t first, const uint32_t second) {
    return contains(adjacency.segment(first), second);
}

// Four quads of a 3x3 grid, one cluster each, all meeting at the centre vertex 4.
Clustering make_quad_grid() {
    std::vector<glm::dvec3> positions;
    for (const uint32_t row : range<uint32_t>(3)) {
        for (const uint32_t column : range<uint32_t>(3)) {
            positions.emplace_back(column, row, 0.0);
        }
    }

    return make_clustering(positions, {
        {{0, 1, 4}, {0, 4, 3}},
        {{1, 2, 5}, {1, 5, 4}},
        {{3, 4, 7}, {3, 7, 6}},
        {{4, 5, 8}, {4, 8, 7}},
    });
}

} // namespace

TEST_CASE("compute_cluster_adjacency reports clusters sharing an edge", "[cluster_adjacency]") {
    const ClusterAdjacency adjacency = compute_cluster_adjacency(make_quad_grid());

    // 0 and 1 share the edge 1-4, 0 and 2 share the edge 3-4.
    CHECK(are_neighbours(adjacency, 0, 1));
    CHECK(are_neighbours(adjacency, 0, 2));
}

TEST_CASE("compute_cluster_adjacency reports clusters sharing only a vertex", "[cluster_adjacency]") {
    const ClusterAdjacency adjacency = compute_cluster_adjacency(make_quad_grid());

    // 0 and 3 meet at the centre vertex 4 alone, as do 1 and 2.
    CHECK(are_neighbours(adjacency, 0, 3));
    CHECK(are_neighbours(adjacency, 1, 2));
}

TEST_CASE("compute_cluster_adjacency leaves clusters sharing nothing apart", "[cluster_adjacency]") {
    Clustering clustering = make_quad_grid();
    clustering.positions.insert(clustering.positions.end(), {{10.0, 0.0, 0.0}, {11.0, 0.0, 0.0}, {10.0, 1.0, 0.0}});

    const Clustering detached = make_clustering(clustering.positions, {{{9, 10, 11}}});
    clustering.clusters.push_back(detached.clusters.front());

    const ClusterAdjacency adjacency = compute_cluster_adjacency(clustering);
    CHECK(adjacency.segment(4).empty());
    for (const uint32_t cluster : range<uint32_t>(4)) {
        CHECK_FALSE(are_neighbours(adjacency, cluster, 4));
    }
}

TEST_CASE("compute_cluster_adjacency excludes the cluster itself", "[cluster_adjacency]") {
    const ClusterAdjacency adjacency = compute_cluster_adjacency(make_quad_grid());

    for (const uint32_t cluster : range<uint32_t>(4)) {
        CHECK_FALSE(are_neighbours(adjacency, cluster, cluster));
    }
}

TEST_CASE("compute_cluster_adjacency lists every cluster a vertex is shared with", "[cluster_adjacency]") {
    const ClusterAdjacency adjacency = compute_cluster_adjacency(make_quad_grid());

    // All four quads meet at the centre vertex, so each borders the other three.
    for (const uint32_t cluster : range<uint32_t>(4)) {
        CHECK(adjacency.segment(cluster).size() == 3);
    }
}

TEST_CASE("compute_cluster_adjacency names each neighbour once", "[cluster_adjacency]") {
    // 0 and 1 share two vertices, so a naive gather would list the neighbour twice.
    const Clustering clustering = make_clustering(
        {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}},
        {{{0, 1, 2}}, {{1, 3, 2}}});

    const ClusterAdjacency adjacency = compute_cluster_adjacency(clustering);
    CHECK(adjacency.segment(0).size() == 1);
    CHECK(adjacency.segment(1).size() == 1);
}

TEST_CASE("compute_cluster_adjacency is symmetric", "[cluster_adjacency]") {
    const ClusterAdjacency adjacency = compute_cluster_adjacency(make_quad_grid());

    for (const uint32_t cluster : range<uint32_t>(4)) {
        for (const uint32_t neighbour : adjacency.segment(cluster)) {
            CHECK(are_neighbours(adjacency, neighbour, cluster));
        }
    }
}

TEST_CASE("compute_cluster_adjacency handles an empty clustering", "[cluster_adjacency]") {
    const ClusterAdjacency adjacency = compute_cluster_adjacency(Clustering{});

    CHECK(adjacency.segment_count() == 0);
}