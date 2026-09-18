#include <cstdint>
#include <utility>
#include <vector>

#include <opencv2/opencv.hpp>

#include "../../catch2_helpers.h"
#include "../../opencv_helpers.h"
#include "texture/basis.h"

namespace packer::texture {

TEST_CASE("basis::Reader reads back the images a page was encoded from", "[texture][basis]") {
    const std::vector<cv::Mat> textures = {make_gradient(16, 16), make_gradient(32, 8), make_gradient(8, 24)};

    auto encoded = basis::encode(textures, {});
    REQUIRE(encoded.has_value());

    const auto reader = basis::Reader::open(MaybeOwnedSlice<uint8_t>::from_owned(std::move(*encoded)));
    REQUIRE(reader.has_value());

    REQUIRE(reader->size() == 3);
    CHECK(reader->get_image_size(0) == glm::uvec2(16, 16));
    CHECK(reader->get_image_size(1) == glm::uvec2(32, 8));
    CHECK(reader->get_image_size(2) == glm::uvec2(8, 24));
}

TEST_CASE("basis::Reader decodes an image close to the one it was given", "[texture][basis]") {
    const cv::Mat texture = make_gradient(64, 64);

    auto encoded = basis::encode(texture, {});
    REQUIRE(encoded.has_value());

    const auto reader = basis::Reader::open(MaybeOwnedSlice<uint8_t>::from_owned(std::move(*encoded)));
    REQUIRE(reader.has_value());

    const auto decoded = reader->decode_image(0);
    REQUIRE(decoded.has_value());

    // The codec is lossy, so the pixels only have to stay close.
    CHECK(mat_are_close(*decoded, texture, 12.0));
}

TEST_CASE("basis::encode encodes a page in each LDR format", "[texture][basis]") {
    const basis::EncodeFormat format = GENERATE(basis::EncodeFormat::Etc1s, basis::EncodeFormat::UastcLdr4x4, basis::EncodeFormat::XuastcLdr4x4);
    const cv::Mat texture = make_gradient(64, 64);

    auto encoded = basis::encode(texture, {.format = format});
    REQUIRE(encoded.has_value());

    const auto reader = basis::Reader::open(MaybeOwnedSlice<uint8_t>::from_owned(std::move(*encoded)));
    REQUIRE(reader.has_value());

    const auto decoded = reader->decode_image(0);
    REQUIRE(decoded.has_value());

    CHECK(mat_are_close(*decoded, texture, 12.0));
}

TEST_CASE("basis::encode rejects a quality or effort outside its range", "[texture][basis]") {
    const basis::EncodeOptions options = GENERATE(
        basis::EncodeOptions{.quality = 0},
        basis::EncodeOptions{.quality = 101},
        basis::EncodeOptions{.effort = 11});

    const auto encoded = basis::encode(make_gradient(16, 16), options);

    REQUIRE_FALSE(encoded.has_value());
    CHECK(encoded.error().kind == basis::EncodeError::Kind::InvalidOptions);
}

TEST_CASE("basis::Reader transcodes to BC7 from any block size", "[texture][basis]") {
    const basis::EncodeFormat format = GENERATE(basis::EncodeFormat::XuastcLdr4x4, basis::EncodeFormat::XuastcLdr6x6);

    auto encoded = basis::encode(make_gradient(30, 18), {.format = format});
    REQUIRE(encoded.has_value());

    const auto reader = basis::Reader::open(MaybeOwnedSlice<uint8_t>::from_owned(std::move(*encoded)));
    REQUIRE(reader.has_value());

    // BC7 stores 16 bytes per 4x4 block, and 30x18 texels need 8x5 of them.
    const size_t size = reader->get_transcoded_size(0, basis::TranscodeFormat::Bc7Rgba);
    CHECK(size == 8 * 5 * 16);

    std::vector<uint8_t> buffer(size);
    CHECK(reader->transcode_image(0, basis::TranscodeFormat::Bc7Rgba, buffer).has_value());
}

TEST_CASE("basis::Reader rejects an index past its last image", "[texture][basis]") {
    auto encoded = basis::encode(make_gradient(16, 16), {});
    REQUIRE(encoded.has_value());

    const auto reader = basis::Reader::open(MaybeOwnedSlice<uint8_t>::from_owned(std::move(*encoded)));
    REQUIRE(reader.has_value());
    REQUIRE(reader->size() == 1);

    CHECK_THROWS(reader->get_image_size(1));
    CHECK_THROWS(reader->decode_image(1));
}

TEST_CASE("basis::Reader reads a borrowed page like an owned one", "[texture][basis]") {
    const auto encoded = basis::encode(make_gradient(16, 16), {});
    REQUIRE(encoded.has_value());

    const auto borrowing = basis::Reader::open(MaybeOwnedSlice<uint8_t>::from_ref(*encoded));
    REQUIRE(borrowing.has_value());

    const auto owning = basis::Reader::open(MaybeOwnedSlice<uint8_t>::from_owned(std::vector<uint8_t>(*encoded)));
    REQUIRE(owning.has_value());

    REQUIRE(borrowing->size() == owning->size());
    CHECK(borrowing->get_image_size(0) == owning->get_image_size(0));

    const auto borrowed_image = borrowing->decode_image(0);
    const auto owned_image = owning->decode_image(0);
    REQUIRE(borrowed_image.has_value());
    REQUIRE(owned_image.has_value());
    CHECK(mat_equals(*borrowed_image, *owned_image));
}

}
