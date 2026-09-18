#include <cstdint>

#include "../../catch2_helpers.h"
#include "geometry/codec.h"
#include "mesh/normalize.h"

namespace packer {

TEST_CASE("geometry::decode undoes geometry::encode", "[geometry][codec]") {
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

TEST_CASE("geometry::decode undoes geometry::encode for an empty cluster", "[geometry][codec]") {
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

TEST_CASE("geometry::decode reports a stream that fails to decode", "[geometry][codec]") {
    const geometry::Page page{
        .clusters = {{.vertex_count = 3, .triangle_count = 1}},
        .positions = {{0, 0, 0}, {7, 0, 0}, {0, 7, 0}},
        .position_indices = {0, 1, 2},
        .uvs = {{0, 0}, {65535, 0}, {0, 65535}},
        .triangles = {{0, 1, 2}},
    };
    geometry::EncodedPage encoded = geometry::encode(page);

    SECTION("a cut-short position stream") {
        encoded.positions.pop_back();
    }
    SECTION("a meshlet too short to decode") {
        encoded.meshlets.resize(1);
        encoded.clusters.front().encoded_meshlet_byte_count = 1;
    }

    CHECK(!geometry::decode(encoded).has_value());
}

}