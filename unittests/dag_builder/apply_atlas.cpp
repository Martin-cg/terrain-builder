#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "atlas/apply_atlas.h"

// A node atlas lays out every cluster's triangles in one uv space, in the order they were
// handed to the unwrap, and duplicates a vertex wherever a chart seam runs through it.
TEST_CASE("apply_node_atlas splits a shared atlas back into its clusters", "[atlas][apply_atlas]") {
    Clustering clustering;
    clustering.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}};

    Cluster first;
    first.vertex_indices = {0, 1, 2};
    first.local_triangles = {{0, 1, 2}};

    Cluster second;
    second.vertex_indices = {1, 2, 3};
    second.local_triangles = {{0, 1, 2}};

    clustering.clusters = {first, second};

    // Global vertex 1 sits on a seam, so it arrives as atlas vertices 1 and 4.
    uv::Atlas atlas;
    atlas.vertex_map = {0, 1, 2, 3, 1};
    atlas.triangles = {{0, 1, 2}, {4, 2, 3}};
    atlas.uvs = {{0.0, 0.0}, {0.5, 0.0}, {0.0, 0.5}, {0.5, 0.5}, {1.0, 0.0}};
    atlas.size = {8, 4};

    apply_node_atlas(clustering, std::vector<uint32_t>{0, 1}, atlas);

    CHECK(clustering.clusters[0].vertex_indices == std::vector<uint32_t>{0, 1, 2});
    CHECK(clustering.clusters[0].local_triangles == std::vector<glm::uvec3>{{0, 1, 2}});
    CHECK(clustering.clusters[0].uvs == std::vector<glm::dvec2>{{0.0, 0.0}, {0.5, 0.0}, {0.0, 0.5}});

    // The second cluster indexes its own vertices from zero, and takes the seam duplicate's uv.
    CHECK(clustering.clusters[1].vertex_indices == std::vector<uint32_t>{1, 2, 3});
    CHECK(clustering.clusters[1].local_triangles == std::vector<glm::uvec3>{{0, 1, 2}});
    CHECK(clustering.clusters[1].uvs == std::vector<glm::dvec2>{{1.0, 0.0}, {0.0, 0.5}, {0.5, 0.5}});
}
