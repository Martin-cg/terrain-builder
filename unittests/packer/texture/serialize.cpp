#include <cstdint>
#include <span>
#include <variant>
#include <vector>

#include <opencv2/opencv.hpp>

#include "../../catch2_helpers.h"
#include "../../opencv_helpers.h"
#include "io/envelope.h"
#include "texture/serialize.h"

namespace packer::texture {

TEST_CASE("texture::deserialize reads back the images texture::serialize wrote", "[texture][serialize]") {
    const cv::Mat texture = make_gradient(64, 64);

    const auto bytes_result = serialize(std::span<const cv::Mat>(&texture, 1), {});
    REQUIRE(bytes_result.has_value());
    const auto reader_result = deserialize(*bytes_result);
    REQUIRE(reader_result.has_value());
    REQUIRE(reader_result->size() == 1);

    const auto decoded = reader_result->decode_image(0);
    REQUIRE(decoded.has_value());
    // The codec is lossy, so the pixels only have to stay close.
    CHECK(mat_are_close(*decoded, texture, 12.0));
}

TEST_CASE("texture::deserialize passes an envelope failure through", "[texture][serialize]") {
    using ForeignSchema = io::envelope::PayloadSchema<"OtherPage", io::envelope::Version<1, EncodedPage>>;

    const auto basis_file = basis::encode(make_gradient(16, 16), {});
    REQUIRE(basis_file.has_value());
    const auto bytes_result = io::envelope::serialize<ForeignSchema, 1>(EncodedPage{.basis_file = *basis_file});
    REQUIRE(bytes_result.has_value());

    const auto result = deserialize(*bytes_result);
    REQUIRE(!result.has_value());

    const io::envelope::Error *error = std::get_if<io::envelope::Error>(&result.error());
    REQUIRE(error != nullptr);
    CHECK(error->code == io::envelope::ErrorCode::WrongClassName);
}

}
