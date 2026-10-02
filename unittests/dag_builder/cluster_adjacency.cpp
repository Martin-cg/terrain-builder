#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "cluster.h"
#include "cluster_adjacency.h"
#include "range_utils.h"

namespace {

bool are_neighbours(const ClusterAdjacency &adjacency, const uint32_t first, const uint32_t second) {
    return contains(adjacency.segment(first), second);
}

// Four quads of a 3x3 grid, one cluster each, all meeting at the centre vertex 4.
Clustering make_quad_grid() {
    Clustering clustering;
    clustering.positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}, {2.0, 1.0, 0.0},
        {0.0, 2.0, 0.0}, {1.0, 2.0, 0.0}, {2.0, 2.0, 0.0},
    };
    clustering.clusters = {
        Cluster{.id = 0, .vertex_indices = {0, 1, 4, 3}, .local_triangles = {{0, 1, 2}, {0, 2, 3}}},
        Cluster{.id = 1, .vertex_indices = {1, 2, 5, 4}, .local_triangles = {{0, 1, 2}, {0, 2, 3}}},
        Cluster{.id = 2, .vertex_indices = {3, 4, 7, 6}, .local_triangles = {{0, 1, 2}, {0, 2, 3}}},
        Cluster{.id = 3, .vertex_indices = {4, 5, 8, 7}, .local_triangles = {{0, 1, 2}, {0, 2, 3}}},
    };
    return clustering;
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
    clustering.clusters.push_back(Cluster{.id = 4, .vertex_indices = {9, 10, 11}, .local_triangles = {{0, 1, 2}}});

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
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}};
    clustering.clusters = {
        Cluster{.id = 0, .vertex_indices = {0, 1, 2}, .local_triangles = {{0, 1, 2}}},
        Cluster{.id = 1, .vertex_indices = {1, 3, 2}, .local_triangles = {{0, 1, 2}}},
    };

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

TEST_CASE("count_vertex_adjacent_clusters counts the clusters using each vertex", "[cluster_adjacency]") {
    Clustering clustering = make_quad_grid();
    clustering.positions.push_back({10.0, 0.0, 0.0});

    // Corners belong to one quad, edge midpoints to two, the centre to all four, and the added vertex to none.
    CHECK(count_vertex_adjacent_clusters(clustering) == std::vector<uint32_t>{1, 2, 1, 2, 4, 2, 1, 2, 1, 0});
}

TEST_CASE("count_vertex_adjacent_clusters saturates at the largest count", "[cluster_adjacency]") {
    // 256 triangles fanning around vertex 0, one cluster each, one more than uint8_t can count.
    Clustering clustering;
    clustering.positions.resize(513);
    for (const uint32_t i : range<uint32_t>(256)) {
        clustering.clusters.push_back(Cluster{.id = i, .vertex_indices = {0, 2 * i + 1, 2 * i + 2}, .local_triangles = {{0, 1, 2}}});
    }

    std::vector<uint8_t> expected(513, 1);
    expected[0] = 255;
    CHECK(count_vertex_adjacent_clusters<uint8_t>(clustering) == expected);
}

TEST_CASE("find_shared_cluster_edges reports a shared edge from each side", "[cluster_adjacency]") {
    // Two triangles meeting along the edge 1-2, one cluster each.
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}};
    clustering.clusters = {
        Cluster{.id = 0, .vertex_indices = {0, 1, 2}, .local_triangles = {{0, 1, 2}}},
        Cluster{.id = 1, .vertex_indices = {1, 3, 2}, .local_triangles = {{0, 1, 2}}},
    };

    SECTION("wound as in each cluster") {
        SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
        expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 1, {1, 2}}});
        expected.push_new_segment(std::vector<SharedClusterEdge>{{1, 0, {2, 1}}});
        CHECK(find_shared_cluster_edges(clustering) == expected);
    }

    SECTION("normalized") {
        SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
        expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 1, {1, 2}}});
        expected.push_new_segment(std::vector<SharedClusterEdge>{{1, 0, {1, 2}}});
        CHECK(find_shared_cluster_edges(clustering, /* normalize */ true) == expected);
    }
}

TEST_CASE("find_shared_cluster_edges leaves out clusters meeting only at a vertex", "[cluster_adjacency]") {
    // 0 and 3 meet at the centre vertex 4 alone, as do 1 and 2.
    SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
    expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 1, {1, 4}}, {0, 2, {3, 4}}});
    expected.push_new_segment(std::vector<SharedClusterEdge>{{1, 0, {1, 4}}, {1, 3, {4, 5}}});
    expected.push_new_segment(std::vector<SharedClusterEdge>{{2, 0, {3, 4}}, {2, 3, {4, 7}}});
    expected.push_new_segment(std::vector<SharedClusterEdge>{{3, 1, {4, 5}}, {3, 2, {4, 7}}});
    CHECK(find_shared_cluster_edges(make_quad_grid(), /* normalize */ true) == expected);
}

TEST_CASE("find_shared_cluster_edges leaves out a cluster holding both ends of an edge it does not have", "[cluster_adjacency]") {
    // The edge 0-1 lies between 0 and 1, while 2 wraps around 1 and touches the vertices 0 and 1 through separate triangles.
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.5, 1.0, 0.0}, {0.5, -1.0, 0.0}, {-1.0, 1.0, 0.0}, {2.0, 1.0, 0.0}, {0.5, 2.0, 0.0}};
    clustering.clusters = {
        Cluster{.id = 0, .vertex_indices = {1, 0, 3}, .local_triangles = {{0, 1, 2}}},
        Cluster{.id = 1, .vertex_indices = {0, 1, 2}, .local_triangles = {{0, 1, 2}}},
        Cluster{.id = 2, .vertex_indices = {0, 2, 4, 1, 5, 6}, .local_triangles = {{0, 1, 2}, {3, 4, 1}, {2, 1, 5}, {1, 4, 5}}},
    };

    SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
    expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 1, {0, 1}}});
    expected.push_new_segment(std::vector<SharedClusterEdge>{{1, 0, {0, 1}}, {1, 2, {0, 2}}, {1, 2, {1, 2}}});
    expected.push_new_segment(std::vector<SharedClusterEdge>{{2, 1, {0, 2}}, {2, 1, {1, 2}}});
    CHECK(find_shared_cluster_edges(clustering, /* normalize */ true) == expected);
}

TEST_CASE("find_shared_cluster_edges orders each cluster's edges", "[cluster_adjacency]") {
    // The bottom row of quads of a 3x3 grid, its triangles listed so that it meets the edge 4-5 before 3-4.
    Clustering clustering;
    clustering.positions = {
        {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0},
        {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}, {2.0, 1.0, 0.0},
        {0.0, 2.0, 0.0}, {1.0, 2.0, 0.0}, {2.0, 2.0, 0.0},
    };
    const Cluster bottom_row{.id = 0, .vertex_indices = {1, 2, 5, 4, 0, 3}, .local_triangles = {{0, 1, 2}, {0, 2, 3}, {4, 0, 3}, {4, 3, 5}}};

    SECTION("by edge, for a neighbour sharing several") {
        clustering.clusters = {
            bottom_row,
            Cluster{.id = 1, .vertex_indices = {3, 4, 7, 6, 5, 8}, .local_triangles = {{0, 1, 2}, {0, 2, 3}, {1, 4, 5}, {1, 5, 2}}},
        };

        SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
        expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 1, {3, 4}}, {0, 1, {4, 5}}});
        expected.push_new_segment(std::vector<SharedClusterEdge>{{1, 0, {3, 4}}, {1, 0, {4, 5}}});
        CHECK(find_shared_cluster_edges(clustering, /* normalize */ true) == expected);
    }

    SECTION("by neighbour") {
        clustering.clusters = {
            bottom_row,
            Cluster{.id = 1, .vertex_indices = {3, 4, 7, 6}, .local_triangles = {{0, 1, 2}, {0, 2, 3}}},
            Cluster{.id = 2, .vertex_indices = {4, 5, 8, 7}, .local_triangles = {{0, 1, 2}, {0, 2, 3}}},
        };

        SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
        expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 1, {3, 4}}, {0, 2, {4, 5}}});
        expected.push_new_segment(std::vector<SharedClusterEdge>{{1, 0, {3, 4}}, {1, 2, {4, 7}}});
        expected.push_new_segment(std::vector<SharedClusterEdge>{{2, 0, {4, 5}}, {2, 1, {4, 7}}});
        CHECK(find_shared_cluster_edges(clustering, /* normalize */ true) == expected);
    }
}

TEST_CASE("find_shared_cluster_edges keeps an empty segment for a cluster without neighbours", "[cluster_adjacency]") {
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}, {10.0, 0.0, 0.0}, {11.0, 0.0, 0.0}, {10.0, 1.0, 0.0}};
    clustering.clusters = {
        Cluster{.id = 0, .vertex_indices = {0, 1, 2}, .local_triangles = {{0, 1, 2}}},
        Cluster{.id = 1, .vertex_indices = {4, 5, 6}, .local_triangles = {{0, 1, 2}}},
        Cluster{.id = 2, .vertex_indices = {1, 3, 2}, .local_triangles = {{0, 1, 2}}},
    };

    SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
    expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 2, {1, 2}}});
    expected.push_new_segment(std::vector<SharedClusterEdge>{});
    expected.push_new_segment(std::vector<SharedClusterEdge>{{2, 0, {1, 2}}});
    CHECK(find_shared_cluster_edges(clustering, /* normalize */ true) == expected);
}

TEST_CASE("find_shared_cluster_edges handles an empty clustering", "[cluster_adjacency]") {
    CHECK(find_shared_cluster_edges(Clustering{}).segment_count() == 0);
}

TEST_CASE("find_shared_cluster_edges handles a non-manifold edge", "[cluster_adjacency]") {
    // Three triangles fanning out from the edge 0-1.
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.5, 1.0, 0.0}, {0.5, -1.0, 0.0}, {0.5, 0.0, 1.0}};

    SECTION("pairing every cluster on it") {
        clustering.clusters = {
            Cluster{.id = 0, .vertex_indices = {0, 1, 2}, .local_triangles = {{0, 1, 2}}},
            Cluster{.id = 1, .vertex_indices = {1, 0, 3}, .local_triangles = {{0, 1, 2}}},
            Cluster{.id = 2, .vertex_indices = {0, 1, 4}, .local_triangles = {{0, 1, 2}}},
        };

        SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
        expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 1, {0, 1}}, {0, 2, {0, 1}}});
        expected.push_new_segment(std::vector<SharedClusterEdge>{{1, 0, {0, 1}}, {1, 2, {0, 1}}});
        expected.push_new_segment(std::vector<SharedClusterEdge>{{2, 0, {0, 1}}, {2, 1, {0, 1}}});
        CHECK(find_shared_cluster_edges(clustering, /* normalize */ true) == expected);
    }

    SECTION("held by one cluster in both windings, which reports it normalized") {
        clustering.clusters = {
            Cluster{.id = 0, .vertex_indices = {0, 1, 2, 3}, .local_triangles = {{0, 1, 2}, {1, 0, 3}}},
            Cluster{.id = 1, .vertex_indices = {1, 0, 4}, .local_triangles = {{0, 1, 2}}},
        };

        SegmentedBuffer<SharedClusterEdge, uint32_t> expected;
        expected.push_new_segment(std::vector<SharedClusterEdge>{{0, 1, {0, 1}}});
        expected.push_new_segment(std::vector<SharedClusterEdge>{{1, 0, {1, 0}}});
        CHECK(find_shared_cluster_edges(clustering) == expected);
    }
}
