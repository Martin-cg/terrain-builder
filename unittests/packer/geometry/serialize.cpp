#include <span>
#include <utility>
#include <variant>

#include "../../catch2_helpers.h"
#include "cluster.h"
#include "geometry/build.h"
#include "geometry/codec.h"
#include "geometry/serialize.h"
#include "io/envelope.h"
#include "mesh/normalize.h"

namespace packer {

TEST_CASE("geometry::deserialize reads back the page geometry::serialize wrote", "[geometry][serialize]") {
    Clustering clustering;
    clustering.positions = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};
    Cluster triangle;
    triangle.vertex_indices = {0, 1, 2};
    triangle.local_triangles = {{0, 1, 2}};
    triangle.uvs = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}};
    clustering.clusters.push_back(std::move(triangle));
    geometry::Page expected = geometry::build(std::span<const Clustering>(&clustering, 1));

    const auto bytes_result = geometry::serialize(expected);
    REQUIRE(bytes_result.has_value());
    auto result = geometry::deserialize(*bytes_result);
    REQUIRE(result.has_value());

    // The encoder may rotate a triangle's corners, so both sides are compared with their smallest index first.
    mesh::normalize_triangles_inplace(expected.triangles);
    mesh::normalize_triangles_inplace(result->triangles);
    CHECK(*result == expected);
}

TEST_CASE("geometry::deserialize passes an envelope failure through", "[geometry][serialize]") {
    using ForeignSchema = io::envelope::PayloadSchema<"OtherPage", io::envelope::Version<1, geometry::EncodedPage>>;

    const geometry::Page page{
        .clusters = {{.vertex_count = 3, .triangle_count = 1}},
        .positions = {{0, 0, 0}, {7, 0, 0}, {0, 7, 0}},
        .position_indices = {0, 1, 2},
        .uvs = {{0, 0}, {65535, 0}, {0, 65535}},
        .triangles = {{0, 1, 2}},
    };
    const auto bytes_result = io::envelope::serialize<ForeignSchema, 1>(geometry::encode(page));
    REQUIRE(bytes_result.has_value());

    const auto result = geometry::deserialize(*bytes_result);
    REQUIRE(!result.has_value());

    const io::envelope::Error *error = std::get_if<io::envelope::Error>(&result.error());
    REQUIRE(error != nullptr);
    CHECK(error->code == io::envelope::ErrorCode::WrongClassName);
}

}