#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "../catch2_helpers.h"
#include "../opencv_helpers.h"

#include "bake/interpolate_uncovered.h"
#include "range_utils.h"

namespace {

// An image and the coverage the bake left on it, so the fixtures read as one grid.
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

// A patch covering the whole grid, as two right triangles over its corners.
std::vector<bake::UvTriangle> make_full_patch() {
    return {
        {{{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}},
        {{{0.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}}}};
}

} // namespace

TEST_CASE("interpolate_uncovered fills a hole from the colours around it", "[bake][interpolate_uncovered]") {
    Input input = make_input(glm::uvec2(5));
    for (const uint32_t y : range<uint32_t>(5)) {
        for (const uint32_t x : range<uint32_t>(5)) {
            if (x != 2 || y != 2) {
                cover<uint8_t>(input, glm::uvec2(x, y), 100);
            }
        }
    }

    const std::vector<bake::UvTriangle> patch = make_full_patch();
    const uint32_t filled = bake::interpolate_uncovered(input.image, input.coverage, patch);

    CHECK(filled == 1);
    CHECK(input.image.at<uint8_t>(2, 2) == 100);
    CHECK(input.coverage.count() == 25);
}

TEST_CASE("interpolate_uncovered carries a gradient across the hole", "[bake][interpolate_uncovered]") {
    Input input = make_input(glm::uvec2(5, 1));
    cover<uint8_t>(input, glm::uvec2(0, 0), 0);
    cover<uint8_t>(input, glm::uvec2(1, 0), 0);
    cover<uint8_t>(input, glm::uvec2(3, 0), 200);
    cover<uint8_t>(input, glm::uvec2(4, 0), 200);

    const std::vector<bake::UvTriangle> patch = make_full_patch();
    const uint32_t filled = bake::interpolate_uncovered(input.image, input.coverage, patch);

    // The hole sits between both sides, so it lands between them rather than on one.
    CHECK(filled == 1);
    CHECK(input.image.at<uint8_t>(0, 2) > 0);
    CHECK(input.image.at<uint8_t>(0, 2) < 200);
}

TEST_CASE("interpolate_uncovered leaves texels outside the patch alone", "[bake][interpolate_uncovered]") {
    Input input = make_input(glm::uvec2(4));
    cover<uint8_t>(input, glm::uvec2(0, 0), 100);

    // A patch covering only the left half, so the right half is nobody's texel.
    const std::vector<bake::UvTriangle> patch = {
        {{{0.0, 0.0}, {0.5, 0.0}, {0.0, 1.0}}}};
    const uint32_t filled = bake::interpolate_uncovered(input.image, input.coverage, patch);

    // The patch holds the texel centres (0,0), (1,0), (0,1) and (0,2); the first was already covered.
    CHECK(input.image.at<uint8_t>(0, 3) == 0);
    CHECK_FALSE(input.coverage.get(3, 0));
    CHECK(filled == 3);
    CHECK(input.coverage.count() == 4);
}

TEST_CASE("interpolate_uncovered does nothing without a patch", "[bake][interpolate_uncovered]") {
    Input input = make_input(glm::uvec2(3));
    cover<uint8_t>(input, glm::uvec2(0, 0), 100);
    const cv::Mat image_before = input.image.clone();

    const uint32_t filled = bake::interpolate_uncovered(
        input.image, input.coverage, std::vector<bake::UvTriangle>{});

    CHECK(filled == 0);
    CHECK(mat_equals(input.image, image_before));
}