#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "cluster.h"
#include "partition.h"
#include "partitioning/provider/Meshopt.h"
#include "partitioning/provider/Metis.h"

TEMPLATE_TEST_CASE("create_partitioning assigns every cluster to one of its partitions, leaving none empty", "[partition]", partitioning::provider::Meshopt, partitioning::provider::Metis) {
    Clustering clustering;

    SECTION("for two pieces far apart, each two triangles meeting along an edge, one cluster per triangle") {
        clustering.positions = {
            {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0},
            {100.0, 0.0, 0.0}, {101.0, 0.0, 0.0}, {100.0, 1.0, 0.0}, {101.0, 1.0, 0.0},
        };
        clustering.clusters = {
            Cluster{.id = 0, .vertex_indices = {0, 1, 2}, .local_triangles = {{0, 1, 2}}},
            Cluster{.id = 1, .vertex_indices = {1, 3, 2}, .local_triangles = {{0, 1, 2}}},
            Cluster{.id = 2, .vertex_indices = {4, 5, 6}, .local_triangles = {{0, 1, 2}}},
            Cluster{.id = 3, .vertex_indices = {5, 7, 6}, .local_triangles = {{0, 1, 2}}},
        };
    }

    SECTION("for four triangles sharing no edges, one cluster per triangle") {
        clustering.positions = {
            {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
            {10.0, 0.0, 0.0}, {11.0, 0.0, 0.0}, {10.0, 1.0, 0.0},
            {20.0, 0.0, 0.0}, {21.0, 0.0, 0.0}, {20.0, 1.0, 0.0},
            {30.0, 0.0, 0.0}, {31.0, 0.0, 0.0}, {30.0, 1.0, 0.0},
        };
        clustering.clusters = {
            Cluster{.id = 0, .vertex_indices = {0, 1, 2}, .local_triangles = {{0, 1, 2}}},
            Cluster{.id = 1, .vertex_indices = {3, 4, 5}, .local_triangles = {{0, 1, 2}}},
            Cluster{.id = 2, .vertex_indices = {6, 7, 8}, .local_triangles = {{0, 1, 2}}},
            Cluster{.id = 3, .vertex_indices = {9, 10, 11}, .local_triangles = {{0, 1, 2}}},
        };
    }

    const Partitioning partitioning = create_partitioning(clustering, PartitionOptions{.clusters_per_partition = 2}, TestType{});

    CHECK(partitioning.clusters_per_partition == 2);
    REQUIRE(partitioning.cluster_partitions.size() == 4);
    std::vector<bool> used(partitioning.partition_count, false);
    for (const uint32_t partition : partitioning.cluster_partitions) {
        REQUIRE(partition < partitioning.partition_count);
        used[partition] = true;
    }
    CHECK(used == std::vector<bool>(partitioning.partition_count, true));
}
