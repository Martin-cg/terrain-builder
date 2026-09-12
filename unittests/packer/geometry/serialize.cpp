#include <variant>

#include "../../catch2_helpers.h"
#include "geometry/codec.h"
#include "geometry/serialize.h"
#include "io/envelope.h"

namespace packer {

TEST_CASE("geometry::read reads a page written through the envelope", "[geometry]") {
    const geometry::Page page{
        .clusters = {{.vertex_count = 3, .triangle_count = 1}},
        .positions = {{0, 0, 0}, {7, 0, 0}, {0, 7, 0}},
        .position_indices = {0, 1, 2},
        .uvs = {{0, 0}, {65535, 0}, {0, 65535}},
        .triangles = {{0, 1, 2}},
    };
    const geometry::EncodedPage encoded = geometry::encode(page);

    const auto bytes_result = io::envelope::serialize<geometry::EncodedPageSchema, 1>(encoded);
    REQUIRE(bytes_result.has_value());

    const auto decoded_result = geometry::decode(encoded);
    REQUIRE(decoded_result.has_value());

    const auto result = geometry::read(*bytes_result);
    REQUIRE(result.has_value());
    CHECK(*result == *decoded_result);
}

TEST_CASE("geometry::read passes an envelope failure through", "[geometry]") {
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

    const auto result = geometry::read(*bytes_result);
    REQUIRE(!result.has_value());

    const io::envelope::Error *error = std::get_if<io::envelope::Error>(&result.error());
    REQUIRE(error != nullptr);
    CHECK(error->code == io::envelope::ErrorCode::WrongClassName);
}

}