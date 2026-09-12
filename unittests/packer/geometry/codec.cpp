#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "../../catch2_helpers.h"
#include "cluster.h"
#include "geometry/codec.h"
#include "mesh/normalize.h"
#include "meshopt/encode.h"

namespace packer {

TEST_CASE("geometry::decode undoes geometry::encode", "[geometry]") {
    const int64_t offset = int64_t{1} << 40;
    geometry::Page expected{
        .clusters = {{.vertex_count = 3, .triangle_count = 1}, {.vertex_count = 4, .triangle_count = 2}},
        .positions = {{0, 0, 0}, {7, 0, 0}, {0, 7, 0}, {offset, 0, 3}, {offset, 7, 3}},
        .position_indices = {0, 1, 2, 1, 3, 2, 4},
        .uvs = {{0, 0}, {65535, 0}, {0, 65535}, {100, 200}, {300, 200}, {100, 400}, {300, 400}},
        .triangles = {{0, 1, 2}, {0, 1, 2}, {1, 3, 2}},
    };

    const auto result = geometry::decode(geometry::encode(expected));
    REQUIRE(result.has_value());
    geometry::Page actual = *result;

    // The encoder may rotate a triangle's corners, so both sides are compared with their smallest index first.
    mesh::normalize_triangles_inplace(expected.triangles);
    mesh::normalize_triangles_inplace(actual.triangles);
    CHECK(actual == expected);
}

TEST_CASE("geometry::decode undoes geometry::encode for an empty cluster", "[geometry]") {
    geometry::Page expected{
        .clusters = {{.vertex_count = 3, .triangle_count = 1}, {.vertex_count = 0, .triangle_count = 0}},
        .positions = {{0, 0, 0}, {7, 0, 0}, {0, 7, 0}},
        .position_indices = {0, 1, 2},
        .uvs = {{0, 0}, {65535, 0}, {0, 65535}},
        .triangles = {{0, 1, 2}},
    };

    const auto result = geometry::decode(geometry::encode(expected));
    REQUIRE(result.has_value());
    geometry::Page actual = *result;

    mesh::normalize_triangles_inplace(expected.triangles);
    mesh::normalize_triangles_inplace(actual.triangles);
    CHECK(actual == expected);
}

TEST_CASE("geometry::decode rejects a page it cannot trust", "[geometry]") {
    const geometry::Page page{
        .clusters = {{.vertex_count = 3, .triangle_count = 1}},
        .positions = {{0, 0, 0}, {7, 0, 0}, {0, 7, 0}},
        .position_indices = {0, 1, 2},
        .uvs = {{0, 0}, {65535, 0}, {0, 65535}},
        .triangles = {{0, 1, 2}},
    };
    geometry::EncodedPage encoded = geometry::encode(page);

    SECTION("meshlet sizes that overrun the meshlet stream") {
        encoded.meshlets.pop_back();
    }
    SECTION("a cluster with more triangles than a cluster can hold") {
        encoded.clusters.front().triangle_count = MAX_TRIANGLES_PER_CLUSTER + 1;
    }
    SECTION("a cut-short position stream") {
        encoded.positions.pop_back();
    }
    SECTION("more positions than the clusters reference") {
        encoded.position_count = encoded.clusters.front().vertex_count + 1;
    }
    SECTION("a meshlet too short to decode") {
        encoded.meshlets.resize(1);
        encoded.clusters.front().encoded_meshlet_byte_count = 1;
    }
    SECTION("a triangle index outside its cluster") {
        const std::array<uint32_t, 3> position_indices = {0, 1, 2};
        const std::array<glm::u8vec3, 1> triangles = {glm::u8vec3{0, 1, 3}};
        encoded.meshlets = meshopt::encode_meshlet(position_indices, triangles);
        encoded.clusters.front().encoded_meshlet_byte_count = static_cast<uint16_t>(encoded.meshlets.size());
    }
    SECTION("a position index beyond the page's positions") {
        const std::array<uint32_t, 3> position_indices = {0, 1, 3};
        const std::array<glm::u8vec3, 1> triangles = {glm::u8vec3{0, 1, 2}};
        encoded.meshlets = meshopt::encode_meshlet(position_indices, triangles);
        encoded.clusters.front().encoded_meshlet_byte_count = static_cast<uint16_t>(encoded.meshlets.size());
    }

    CHECK(!geometry::decode(encoded).has_value());
}

}