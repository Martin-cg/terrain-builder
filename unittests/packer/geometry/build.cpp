#include <algorithm>
#include <array>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "../../catch2_helpers.h"
#include "cluster.h"
#include "containers/OffsetTable.h"
#include "enumerate.h"
#include "geometry/build.h"
#include "glm_utils.h"
#include "mesh/normalize.h"
#include "range_utils.h"

namespace packer {

namespace {

// The triangles of cluster `cluster_index` as page vertex indices, rotated to their smallest index and sorted.
std::vector<glm::uvec3> get_cluster_triangles(const geometry::detail::PageMeshlets &meshlets, const uint32_t cluster_index) {
    const std::span<const uint32_t> vertex_indices = meshlets.vertex_indices.segment(cluster_index);
    std::vector<glm::uvec3> triangles = transform_vector(meshlets.local_triangles.segment(cluster_index), [&](const glm::u8vec3 &t) {
        return transform(t, [&](const uint8_t i) { return vertex_indices[i]; });
    });
    mesh::sort_and_normalize_triangles(triangles);
    return triangles;
}

using PageVertex = std::pair<QuantizedPoint, geometry::QuantizedUv>;

// The triangles of cluster `cluster_index` of `page`, each corner replaced by the index of its position and UV in `vertices`, rotated to their smallest index and sorted.
std::vector<glm::uvec3> get_page_triangles(const geometry::Page &page, const size_t cluster_index, const std::span<const PageVertex> vertices) {
    const OffsetTable vertex_offsets = build_offset_table(page.clusters | std::views::transform(&geometry::ClusterHeader::vertex_count));
    const OffsetTable triangle_offsets = build_offset_table(page.clusters | std::views::transform(&geometry::ClusterHeader::triangle_count));
    const size_t vertex_begin = vertex_offsets.get_begin(cluster_index);
    const std::span<const glm::u8vec3> local_triangles = std::span(page.triangles).subspan(
        triangle_offsets.get_begin(cluster_index),
        triangle_offsets.segment_size(cluster_index));
    std::vector<glm::uvec3> triangles = transform_vector(local_triangles, [&](const glm::u8vec3 &t) {
        return transform(t, [&](const uint8_t i) {
            const PageVertex vertex{page.positions[page.position_indices[vertex_begin + i]], page.uvs[vertex_begin + i]};
            return static_cast<uint32_t>(std::ranges::find(vertices, vertex) - vertices.begin());
        });
    });
    mesh::sort_and_normalize_triangles(triangles);
    return triangles;
}

}

TEST_CASE("geometry::detail::quantize_vertices keeps cluster order and clamps UVs", "[geometry]") {
    Clustering first;
    first.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    Cluster first_cluster;
    first_cluster.vertex_indices = {2, 0, 1};
    first_cluster.local_triangles = {{0, 1, 2}};
    first_cluster.uvs = {{0.5, 0.5}, {-0.25, 0.0}, {1.0, 1.5}};
    first.clusters.push_back(std::move(first_cluster));
    Clustering second;
    second.positions = {{5.0, 5.0, 5.0}};
    Cluster second_cluster;
    second_cluster.vertex_indices = {0};
    second_cluster.uvs = {{0.25, 0.75}};
    second.clusters.push_back(std::move(second_cluster));
    const std::array<Clustering, 2> clusterings = {std::move(first), std::move(second)};

    const geometry::detail::PageVertices vertices = geometry::detail::quantize_vertices(clusterings);

    CHECK(vertices.positions == std::vector<QuantizedPoint>{
        quantize_point({0.0, 1.0, 0.0}), quantize_point({0.0, 0.0, 0.0}), quantize_point({1.0, 0.0, 0.0}), quantize_point({5.0, 5.0, 5.0})});
    CHECK(vertices.uvs == std::vector<geometry::QuantizedUv>{
        geometry::quantize_uv({0.5, 0.5}), geometry::quantize_uv({0.0, 0.0}), geometry::quantize_uv({1.0, 1.0}), geometry::quantize_uv({0.25, 0.75})});
}

TEST_CASE("geometry::detail::deduplicate_positions stores each position once", "[geometry]") {
    const std::vector<QuantizedPoint> positions = {{0, 0, 0}, {7, 0, 0}, {0, 0, 0}, {0, 7, 0}, {7, 0, 0}};

    const geometry::detail::DeduplicatePositionsResult deduplicated = geometry::detail::deduplicate_positions(positions);

    CHECK(deduplicated.positions.size() == 3);
    REQUIRE(deduplicated.position_ids.size() == positions.size());
    for (const auto &[index, id] : enumerate(deduplicated.position_ids)) {
        CHECK(deduplicated.positions[id] == positions[index]);
    }
}

TEST_CASE("geometry::detail::build_meshlets keeps every cluster's triangles", "[geometry]") {
    Clustering first;
    first.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}};
    Cluster quad;
    quad.vertex_indices = {0, 1, 2, 3};
    quad.local_triangles = {{0, 1, 2}, {0, 2, 3}};
    quad.uvs = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    first.clusters.push_back(std::move(quad));
    Cluster triangle;
    triangle.vertex_indices = {1, 2, 3};
    triangle.local_triangles = {{0, 1, 2}};
    triangle.uvs = {{0.5, 0.0}, {0.5, 0.5}, {0.0, 0.5}};
    first.clusters.push_back(std::move(triangle));
    Clustering second;
    second.positions = {{5.0, 0.0, 0.0}, {6.0, 0.0, 0.0}, {5.0, 1.0, 0.0}};
    Cluster far_triangle;
    far_triangle.vertex_indices = {0, 1, 2};
    far_triangle.local_triangles = {{0, 1, 2}};
    far_triangle.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    second.clusters.push_back(std::move(far_triangle));
    const std::array<Clustering, 2> clusterings = {std::move(first), std::move(second)};
    const geometry::detail::PageVertices vertices = geometry::detail::quantize_vertices(clusterings);
    const geometry::detail::DeduplicatePositionsResult deduplicated = geometry::detail::deduplicate_positions(vertices.positions);

    const geometry::detail::PageMeshlets meshlets = geometry::detail::build_meshlets(clusterings, vertices, deduplicated.position_ids);

    REQUIRE(meshlets.vertex_indices.segment_count() == 3);
    REQUIRE(meshlets.local_triangles.segment_count() == 3);
    CHECK(get_cluster_triangles(meshlets, 0) == std::vector<glm::uvec3>{{0, 1, 2}, {0, 2, 3}});
    CHECK(get_cluster_triangles(meshlets, 1) == std::vector<glm::uvec3>{{4, 5, 6}});
    CHECK(get_cluster_triangles(meshlets, 2) == std::vector<glm::uvec3>{{7, 8, 9}});
}

TEST_CASE("geometry::detail::build_meshlets drops triangles whose corners share a position", "[geometry]") {
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 0.0}};
    Cluster cluster;
    cluster.vertex_indices = {0, 1, 2, 3};
    cluster.local_triangles = {{0, 1, 2}, {0, 1, 3}, {3, 1, 2}};
    cluster.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {0.5, 0.5}};
    clustering.clusters.push_back(std::move(cluster));
    const std::span<const Clustering> clusterings(&clustering, 1);
    const geometry::detail::PageVertices vertices = geometry::detail::quantize_vertices(clusterings);
    const geometry::detail::DeduplicatePositionsResult deduplicated = geometry::detail::deduplicate_positions(vertices.positions);

    const geometry::detail::PageMeshlets meshlets = geometry::detail::build_meshlets(clusterings, vertices, deduplicated.position_ids);

    CHECK(get_cluster_triangles(meshlets, 0) == std::vector<glm::uvec3>{{0, 1, 2}});
    CHECK(meshlets.vertex_indices.segment_size(0) == 3);
}

TEST_CASE("geometry::detail::build_meshlets keeps a cluster whose triangles all collapse, as an empty cluster", "[geometry]") {
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {5.0, 5.0, 5.0}};
    Cluster triangle;
    triangle.vertex_indices = {0, 1, 2};
    triangle.local_triangles = {{0, 1, 2}};
    triangle.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    clustering.clusters.push_back(std::move(triangle));
    Cluster collapsed;
    collapsed.vertex_indices = {3, 3, 3};
    collapsed.local_triangles = {{0, 1, 2}};
    collapsed.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    clustering.clusters.push_back(std::move(collapsed));
    const std::span<const Clustering> clusterings(&clustering, 1);
    const geometry::detail::PageVertices vertices = geometry::detail::quantize_vertices(clusterings);
    const geometry::detail::DeduplicatePositionsResult deduplicated = geometry::detail::deduplicate_positions(vertices.positions);

    const geometry::detail::PageMeshlets meshlets = geometry::detail::build_meshlets(clusterings, vertices, deduplicated.position_ids);

    REQUIRE(meshlets.vertex_indices.segment_count() == 2);
    CHECK(get_cluster_triangles(meshlets, 0) == std::vector<glm::uvec3>{{0, 1, 2}});
    CHECK(meshlets.vertex_indices.segment_size(1) == 0);
    CHECK(meshlets.local_triangles.segment_size(1) == 0);
}

TEST_CASE("geometry::build keeps each cluster's triangle corners", "[geometry]") {
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}};
    Cluster left;
    left.vertex_indices = {0, 1, 2};
    left.local_triangles = {{0, 1, 2}};
    left.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    clustering.clusters.push_back(std::move(left));
    Cluster right;
    right.vertex_indices = {1, 3, 2};
    right.local_triangles = {{0, 1, 2}};
    right.uvs = {{0.25, 0.25}, {0.75, 0.25}, {0.25, 0.75}};
    clustering.clusters.push_back(std::move(right));
    const std::vector<PageVertex> vertices = {
        {quantize_point({0.0, 0.0, 0.0}), geometry::quantize_uv({0.0, 0.0})},
        {quantize_point({1.0, 0.0, 0.0}), geometry::quantize_uv({1.0, 0.0})},
        {quantize_point({0.0, 1.0, 0.0}), geometry::quantize_uv({0.0, 1.0})},
        {quantize_point({1.0, 0.0, 0.0}), geometry::quantize_uv({0.25, 0.25})},
        {quantize_point({1.0, 1.0, 0.0}), geometry::quantize_uv({0.75, 0.25})},
        {quantize_point({0.0, 1.0, 0.0}), geometry::quantize_uv({0.25, 0.75})},
    };

    const geometry::Page page = geometry::build(std::span<const Clustering>(&clustering, 1));

    REQUIRE(page.clusters.size() == 2);
    CHECK(get_page_triangles(page, 0, vertices) == std::vector<glm::uvec3>{{0, 1, 2}});
    CHECK(get_page_triangles(page, 1, vertices) == std::vector<glm::uvec3>{{3, 4, 5}});
}

TEST_CASE("geometry::build stores a position shared by two clusterings once", "[geometry]") {
    Clustering first;
    first.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    Cluster first_cluster;
    first_cluster.vertex_indices = {0, 1, 2};
    first_cluster.local_triangles = {{0, 1, 2}};
    first_cluster.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    first.clusters.push_back(std::move(first_cluster));
    Clustering second;
    second.positions = {{1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {1.0, 1.0, 0.0}};
    Cluster second_cluster;
    second_cluster.vertex_indices = {0, 1, 2};
    second_cluster.local_triangles = {{0, 1, 2}};
    second_cluster.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    second.clusters.push_back(std::move(second_cluster));
    const std::array<Clustering, 2> clusterings = {std::move(first), std::move(second)};

    const geometry::Page page = geometry::build(clusterings);

    CHECK(page.positions.size() == 5);
    CHECK(std::ranges::count(page.positions, quantize_point({1.0, 0.0, 0.0})) == 1);
}

TEST_CASE("geometry::build produces a page that survives encode and decode", "[geometry]") {
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0}};
    Cluster quad;
    quad.vertex_indices = {0, 1, 3, 2};
    quad.local_triangles = {{0, 1, 2}, {0, 2, 3}};
    quad.uvs = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    clustering.clusters.push_back(std::move(quad));
    Cluster triangle;
    triangle.vertex_indices = {1, 3, 2};
    triangle.local_triangles = {{0, 1, 2}};
    triangle.uvs = {{0.25, 0.25}, {0.75, 0.25}, {0.25, 0.75}};
    clustering.clusters.push_back(std::move(triangle));
    geometry::Page expected = geometry::build(std::span<const Clustering>(&clustering, 1));

    const auto result = geometry::decode(geometry::encode(expected));
    REQUIRE(result.has_value());
    geometry::Page actual = *result;

    // The encoder may rotate a triangle's corners, so both sides are compared with their smallest index first.
    mesh::normalize_triangles_inplace(expected.triangles);
    mesh::normalize_triangles_inplace(actual.triangles);
    CHECK(actual == expected);
}

}