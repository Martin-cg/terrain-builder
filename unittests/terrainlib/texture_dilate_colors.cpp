#include <cstdint>
#include <initializer_list>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "range_utils.h"
#include "texture/dilate_colors.h"

namespace {

// An image and the coverage it is dilated against, so the fixtures read as one grid.
struct Input {
    cv::Mat image;
    texture::Mask coverage;
};

Input make_input(const glm::uvec2 size, const int type = CV_8UC1) {
    return {
        cv::Mat(size.y, size.x, type, cv::Scalar::all(0)),
        texture::Mask(size)};
}

template <typename Pixel>
void cover(Input &input, const glm::uvec2 at, const Pixel value) {
    input.image.at<Pixel>(at.y, at.x) = value;
    input.coverage.set(at.x, at.y);
}

// A single-row target, named by the columns it admits.
texture::Mask make_target(const glm::uvec2 size, const std::initializer_list<uint32_t> columns) {
    texture::Mask target(size);
    for (const uint32_t x : columns) {
        target.set(x, 0);
    }
    return target;
}

} // namespace

TEST_CASE("dilate_colors leaves everything alone for a zero radius", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(5));
    cover<uint8_t>(input, glm::uvec2(2), 100);
    const cv::Mat image_before = input.image.clone();

    texture::dilate_colors_inplace(input.image, input.coverage, 0);

    CHECK(mat_equals(input.image, image_before));
    CHECK(input.coverage.count() == 1);
}

TEST_CASE("dilate_colors grows the covered region by one ring per round", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(5));
    cover<uint8_t>(input, glm::uvec2(2), 100);

    texture::dilate_colors_inplace(input.image, input.coverage, 1);

    // The eight neighbours take the centre's color, and nothing beyond them is touched.
    for (const uint32_t y : range<uint32_t>(5)) {
        for (const uint32_t x : range<uint32_t>(5)) {
            const glm::ivec2 offset = glm::ivec2(x, y) - glm::ivec2(2);
            const bool in_ring = glm::all(glm::lessThanEqual(glm::abs(offset), glm::ivec2(1)));
            REQUIRE(input.coverage.get(x, y) == in_ring);
            REQUIRE(input.image.at<uint8_t>(y, x) == (in_ring ? 100 : 0));
        }
    }
}

TEST_CASE("dilate_colors reaches the whole image given enough rounds", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(5));
    cover<uint8_t>(input, glm::uvec2(2), 100);

    texture::dilate_colors_inplace(input.image, input.coverage, 2);

    for (const uint32_t y : range<uint32_t>(5)) {
        for (const uint32_t x : range<uint32_t>(5)) {
            REQUIRE(input.coverage.get(x, y));
            REQUIRE(input.image.at<uint8_t>(y, x) == 100);
        }
    }
}

TEST_CASE("dilate_colors averages only the covered neighbours", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(3, 1));
    cover<uint8_t>(input, glm::uvec2(0, 0), 40);
    cover<uint8_t>(input, glm::uvec2(2, 0), 200);

    texture::dilate_colors_inplace(input.image, input.coverage, 1);

    CHECK(input.image.at<uint8_t>(0, 1) == 120);
}

TEST_CASE("dilate_colors has nothing to spread when nothing is covered", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(4));
    input.image.setTo(cv::Scalar::all(70));
    const cv::Mat image_before = input.image.clone();

    texture::dilate_colors_inplace(input.image, input.coverage, 3);

    CHECK(mat_equals(input.image, image_before));
    CHECK(input.coverage.count() == 0);
}

TEST_CASE("dilate_colors leaves a fully covered image alone", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(3));
    for (const uint32_t y : range<uint32_t>(3)) {
        for (const uint32_t x : range<uint32_t>(3)) {
            cover<uint8_t>(input, glm::uvec2(x, y), uint8_t(10 * (3 * y + x)));
        }
    }
    const cv::Mat image_before = input.image.clone();

    texture::dilate_colors_inplace(input.image, input.coverage, 2);

    CHECK(mat_equals(input.image, image_before));
}

TEST_CASE("dilate_colors spreads every channel", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(3), CV_8UC3);
    cover<cv::Vec3b>(input, glm::uvec2(1), cv::Vec3b(10, 20, 30));

    texture::dilate_colors_inplace(input.image, input.coverage, 1);

    CHECK(input.image.at<cv::Vec3b>(0, 0) == cv::Vec3b(10, 20, 30));
    CHECK(input.image.at<cv::Vec3b>(2, 2) == cv::Vec3b(10, 20, 30));
}

TEST_CASE("dilate_colors dispatches on the image depth", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(3), CV_32FC1);
    cover<float>(input, glm::uvec2(1), 1.5f);

    texture::dilate_colors_inplace(input.image, input.coverage, 1);

    CHECK(input.image.at<float>(0, 0) == Catch::Approx(1.5));
}

TEST_CASE("dilate_colors only grows into the target", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(5, 1));
    cover<uint8_t>(input, glm::uvec2(0, 0), 100);
    texture::Mask target = make_target(glm::uvec2(5, 1), {1, 2});

    const uint32_t dilated = texture::dilate_colors_inplace(input.image, input.coverage, target);

    CHECK(dilated == 2);
    CHECK(input.image.at<uint8_t>(0, 1) == 100);
    CHECK(input.image.at<uint8_t>(0, 2) == 100);
    CHECK(input.image.at<uint8_t>(0, 3) == 0);
    CHECK_FALSE(input.coverage.get(3, 0));
    CHECK_FALSE(input.coverage.get(4, 0));
}

TEST_CASE("dilate_colors fills the whole target when given no radius", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(5, 1));
    cover<uint8_t>(input, glm::uvec2(0, 0), 100);
    texture::Mask target = make_target(glm::uvec2(5, 1), {1, 2, 3, 4});

    const uint32_t dilated = texture::dilate_colors_inplace(input.image, input.coverage, target);

    CHECK(dilated == 4);
    for (const uint32_t x : range<uint32_t>(5)) {
        REQUIRE(input.coverage.get(x, 0));
        REQUIRE(input.image.at<uint8_t>(0, x) == 100);
    }
}

TEST_CASE("dilate_colors stops the target fill after max_radius rounds", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(5, 1));
    cover<uint8_t>(input, glm::uvec2(0, 0), 100);
    texture::Mask target = make_target(glm::uvec2(5, 1), {1, 2, 3, 4});

    const uint32_t dilated = texture::dilate_colors_inplace(input.image, input.coverage, target, 2);

    CHECK(dilated == 2);
    CHECK(input.coverage.get(2, 0));
    CHECK_FALSE(input.coverage.get(3, 0));
}

TEST_CASE("dilate_colors leaves target texels it cannot reach", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(5, 1));
    cover<uint8_t>(input, glm::uvec2(0, 0), 100);
    // Texel 2 is outside the target, so it walls off the texels past it.
    texture::Mask target = make_target(glm::uvec2(5, 1), {1, 3, 4});

    const uint32_t dilated = texture::dilate_colors_inplace(input.image, input.coverage, target);

    CHECK(dilated == 1);
    CHECK(input.coverage.get(1, 0));
    CHECK_FALSE(input.coverage.get(3, 0));
    CHECK(input.image.at<uint8_t>(0, 3) == 0);
}

TEST_CASE("dilate_colors leaves the input alone when returning a copy", "[texture][dilate_colors]") {
    Input input = make_input(glm::uvec2(3));
    cover<uint8_t>(input, glm::uvec2(1), 100);
    const cv::Mat image_before = input.image.clone();

    const cv::Mat dilated = texture::dilate_colors(input.image, input.coverage, 1);

    CHECK(mat_equals(input.image, image_before));
    CHECK(input.coverage.count() == 1);
    CHECK(dilated.at<uint8_t>(0, 0) == 100);
}

TEST_CASE("fill_uncovered leaves the covered texels alone", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(6, 4));
    cover<uint8_t>(input, glm::uvec2(1, 1), 40);
    cover<uint8_t>(input, glm::uvec2(4, 2), 200);

    texture::fill_uncovered_inplace(input.image, input.coverage);

    CHECK(input.image.at<uint8_t>(1, 1) == 40);
    CHECK(input.image.at<uint8_t>(2, 4) == 200);
}

TEST_CASE("fill_uncovered fills every uncovered texel from the covered colours", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(6, 4));
    // No blend of the covered colours can come out at the clear value.
    input.image.setTo(cv::Scalar::all(255));
    cover<uint8_t>(input, glm::uvec2(0, 0), 40);
    cover<uint8_t>(input, glm::uvec2(5, 3), 80);

    texture::fill_uncovered_inplace(input.image, input.coverage);

    for (const uint32_t y : range<uint32_t>(4)) {
        for (const uint32_t x : range<uint32_t>(6)) {
            REQUIRE(input.image.at<uint8_t>(y, x) >= 40);
            REQUIRE(input.image.at<uint8_t>(y, x) <= 80);
        }
    }
}

TEST_CASE("fill_uncovered has nothing to spread when nothing is covered", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(4));
    input.image.setTo(cv::Scalar::all(70));
    const cv::Mat image_before = input.image.clone();

    texture::fill_uncovered_inplace(input.image, input.coverage);

    CHECK(mat_equals(input.image, image_before));
}

TEST_CASE("fill_uncovered spreads a single covered texel over an odd sized image", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(7, 5));
    cover<uint8_t>(input, glm::uvec2(5, 1), 100);

    texture::fill_uncovered_inplace(input.image, input.coverage);

    for (const uint32_t y : range<uint32_t>(5)) {
        for (const uint32_t x : range<uint32_t>(7)) {
            REQUIRE(input.image.at<uint8_t>(y, x) == 100);
        }
    }
}

TEST_CASE("fill_uncovered keeps colours near where they were covered", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(8, 1));
    cover<uint8_t>(input, glm::uvec2(0, 0), 0);
    cover<uint8_t>(input, glm::uvec2(7, 0), 200);

    texture::fill_uncovered_inplace(input.image, input.coverage);

    for (const uint32_t x : range<uint32_t>(1u, 8u)) {
        REQUIRE(input.image.at<uint8_t>(0, x - 1) <= input.image.at<uint8_t>(0, x));
    }
    CHECK(input.image.at<uint8_t>(0, 1) < 100);
    CHECK(input.image.at<uint8_t>(0, 6) > 100);
}

TEST_CASE("fill_uncovered spreads every channel", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3), CV_8UC3);
    cover<cv::Vec3b>(input, glm::uvec2(1), cv::Vec3b(10, 20, 30));

    texture::fill_uncovered_inplace(input.image, input.coverage);

    CHECK(input.image.at<cv::Vec3b>(0, 0) == cv::Vec3b(10, 20, 30));
    CHECK(input.image.at<cv::Vec3b>(2, 2) == cv::Vec3b(10, 20, 30));
}

TEST_CASE("fill_uncovered dispatches on the image depth", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3), CV_32FC1);
    cover<float>(input, glm::uvec2(1), 1.5f);

    texture::fill_uncovered_inplace(input.image, input.coverage);

    CHECK(input.image.at<float>(0, 0) == Catch::Approx(1.5));
}

TEST_CASE("fill_uncovered leaves the input alone when returning a copy", "[terrainlib][texture_dilate_colors]") {
    Input input = make_input(glm::uvec2(3));
    cover<uint8_t>(input, glm::uvec2(1), 100);
    const cv::Mat image_before = input.image.clone();

    const cv::Mat filled = texture::fill_uncovered(input.image, input.coverage);

    CHECK(mat_equals(input.image, image_before));
    CHECK(filled.at<uint8_t>(0, 0) == 100);
}
