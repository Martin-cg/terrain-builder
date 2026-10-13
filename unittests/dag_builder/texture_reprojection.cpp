#include <opencv2/opencv.hpp>
#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "atlas/texture_reprojection.h"
#include "range_utils.h"
#include "texture/clear_color.h"
#include "texture/dilate_colors.h"

namespace {

cv::Mat make_texture(const cv::Vec3b top_left, const cv::Vec3b top_right) {
    cv::Mat texture(1, 2, CV_8UC3);
    texture.at<cv::Vec3b>(0, 0) = top_left;
    texture.at<cv::Vec3b>(0, 1) = top_right;
    return texture;
}

} // namespace

TEST_CASE("reproject copies pixels under an identity mapping", "[atlas][texture_reprojection]") {
    const cv::Mat source = make_texture(cv::Vec3b(10, 20, 30), cv::Vec3b(40, 50, 60));

    std::array<MappedTriangle, 2> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[1].source_image_index = 0;
    triangles[1].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(0, 1)};
    triangles[1].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(0, 1)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(2, 1), CV_8UC3, DEFAULT_CLEAR_COLOR,
        ReprojectionOptions{1, texture::Interpolation::Nearest});

    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(10, 20, 30));
    CHECK(output.at<cv::Vec3b>(0, 1) == cv::Vec3b(40, 50, 60));
}

TEST_CASE("reproject linearly blends between neighboring source pixels", "[atlas][texture_reprojection]") {
    const cv::Mat source = make_texture(cv::Vec3b(0, 0, 0), cv::Vec3b(200, 200, 200));

    std::array<MappedTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(1, 1), CV_8UC3, DEFAULT_CLEAR_COLOR,
        ReprojectionOptions{1, texture::Interpolation::Linear});

    // The single output sample lands halfway between the two source pixels.
    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(100, 100, 100));
}

TEST_CASE("reproject leaves the background where triangles are degenerate", "[atlas][texture_reprojection]") {
    const cv::Mat source = make_texture(cv::Vec3b(10, 20, 30), cv::Vec3b(40, 50, 60));

    std::array<MappedTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0.5, 0.5), glm::dvec2(0.5, 0.5), glm::dvec2(0.5, 0.5)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(1, 1), CV_8UC3, cv::Scalar::all(0),
        ReprojectionOptions{1, texture::Interpolation::Nearest});

    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(0, 0, 0));
}

TEST_CASE("reproject supersamples and averages partial pixel coverage", "[atlas][texture_reprojection]") {
    // Triangle covering only the lower-left sample at UV (0.25, 0.25)
    cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(100, 100, 100));

    std::array<MappedTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    // Triangle covers lower-left quarter of output pixel
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(0.5, 0), glm::dvec2(0.5, 0.5)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(1, 1), CV_8UC3, DEFAULT_CLEAR_COLOR,
        ReprojectionOptions{2, texture::Interpolation::Nearest});

    // Only 1 of 4 samples is covered, output is average of covered samples
    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(100, 100, 100));
}

TEST_CASE("reproject handles non-uniform coverage with gradient source", "[atlas][texture_reprojection]") {
    // 2x1 gradient source for testing coverage-aware averaging
    cv::Mat source(1, 2, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(50, 50, 50);
    source.at<cv::Vec3b>(0, 1) = cv::Vec3b(150, 150, 150);

    std::array<MappedTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(1, 1), CV_8UC3, DEFAULT_CLEAR_COLOR,
        ReprojectionOptions{2, texture::Interpolation::Linear});

    const auto pixel = output.at<cv::Vec3b>(0, 0);
    // With linear interpolation across gradient, should be intermediate value
    CHECK(pixel[0] > 50);
    CHECK(pixel[0] < 150);
}

TEST_CASE("reproject background stays zero when no samples covered", "[atlas][texture_reprojection]") {
    cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(255, 255, 255));

    std::array<MappedTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    // Triangle outside output area
    triangles[0].target_uvs = {glm::dvec2(2, 2), glm::dvec2(3, 2), glm::dvec2(3, 3)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(1, 1), CV_8UC3, cv::Scalar::all(0),
        ReprojectionOptions{2, texture::Interpolation::Nearest});

    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(0, 0, 0));
}

TEST_CASE("reproject quad seam has no holes with supersampling", "[atlas][texture_reprojection]") {
    // Full coverage with two triangles forming a quad and supersampling
    cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(128, 128, 128));

    std::array<MappedTriangle, 2> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[1].source_image_index = 0;
    triangles[1].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(0, 1)};
    triangles[1].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(0, 1)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(4, 4), CV_8UC3, DEFAULT_CLEAR_COLOR,
        ReprojectionOptions{2, texture::Interpolation::Nearest});

    // Every output pixel must equal source color (no holes on seams)
    for (int y = 0; y < output.rows; y++) {
        for (int x = 0; x < output.cols; x++) {
            REQUIRE(output.at<cv::Vec3b>(y, x) == cv::Vec3b(128, 128, 128));
        }
    }
}

TEST_CASE("reproject renders identically regardless of winding order", "[atlas][texture_reprojection]") {
    cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(64, 64, 64));

    // CCW triangle
    std::array<MappedTriangle, 1> triangles_ccw;
    triangles_ccw[0].source_image_index = 0;
    triangles_ccw[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles_ccw[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    // CW triangle (swap two vertices)
    std::array<MappedTriangle, 1> triangles_cw;
    triangles_cw[0].source_image_index = 0;
    triangles_cw[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(1, 0)};
    triangles_cw[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 1), glm::dvec2(1, 0)};

    const ReprojectionOptions options{1, texture::Interpolation::Nearest};
    const cv::Mat output_ccw = reproject(
        std::span(&source, 1), triangles_ccw, glm::uvec2(2, 2), CV_8UC3, DEFAULT_CLEAR_COLOR, options);
    const cv::Mat output_cw = reproject(
        std::span(&source, 1), triangles_cw, glm::uvec2(2, 2), CV_8UC3, DEFAULT_CLEAR_COLOR, options);

    // Winding repair should make both renders identical
    for (int y = 0; y < output_ccw.rows; y++) {
        for (int x = 0; x < output_ccw.cols; x++) {
            CHECK(output_ccw.at<cv::Vec3b>(y, x) == output_cw.at<cv::Vec3b>(y, x));
        }
    }
}

TEST_CASE("reproject handles multiple source images with different colors", "[atlas][texture_reprojection]") {
    // Two 1x1 sources with different colors
    cv::Mat source0(1, 1, CV_8UC3, cv::Vec3b(255, 0, 0));    // red
    cv::Mat source1(1, 1, CV_8UC3, cv::Vec3b(0, 255, 0));    // green
    std::array<cv::Mat, 2> sources = {source0, source1};

    // Two triangles, each using a different source
    std::array<MappedTriangle, 2> triangles;
    // Left triangle uses source 0 (red)
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(0.5, 0), glm::dvec2(0.5, 1)};
    // Right triangle uses source 1 (green)
    triangles[1].source_image_index = 1;
    triangles[1].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[1].target_uvs = {glm::dvec2(0.5, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    const cv::Mat output = reproject(
        sources, triangles, glm::uvec2(2, 1), CV_8UC3, DEFAULT_CLEAR_COLOR,
        ReprojectionOptions{1, texture::Interpolation::Nearest});

    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(255, 0, 0));    // red
    CHECK(output.at<cv::Vec3b>(0, 1) == cv::Vec3b(0, 255, 0));    // green
}

TEST_CASE("reproject clamps UVs and applies edge replication", "[atlas][texture_reprojection]") {
    cv::Mat source(2, 2, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(255, 0, 0);      // red
    source.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 255, 0);      // green
    source.at<cv::Vec3b>(1, 0) = cv::Vec3b(0, 0, 255);      // blue
    source.at<cv::Vec3b>(1, 1) = cv::Vec3b(255, 255, 0);    // yellow

    std::array<MappedTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    // All source UVs lie outside [0, 1] and get clamped to the (1, 1) corner.
    triangles[0].source_uvs = {glm::dvec2(2, 2), glm::dvec2(3, 2), glm::dvec2(3, 3)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(1, 1), CV_8UC3, DEFAULT_CLEAR_COLOR,
        ReprojectionOptions{1, texture::Interpolation::Nearest});

    // Every sample reads the bottom-right border texel (yellow).
    CHECK(output.at<cv::Vec3b>(0, 0) == cv::Vec3b(255, 255, 0));
}

TEST_CASE("reproject pins rounding behavior for linear interpolation blends", "[atlas][texture_reprojection]") {
    // Create a 2x1 source with black and white to test 127.5 blend
    cv::Mat source(1, 2, CV_8UC3);
    source.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 0, 0);        // black
    source.at<cv::Vec3b>(0, 1) = cv::Vec3b(255, 255, 255);  // white

    std::array<MappedTriangle, 1> triangles;
    triangles[0].source_image_index = 0;
    triangles[0].source_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    triangles[0].target_uvs = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};

    const cv::Mat output = reproject(
        std::span(&source, 1), triangles, glm::uvec2(1, 1), CV_8UC3, DEFAULT_CLEAR_COLOR,
        ReprojectionOptions{1, texture::Interpolation::Linear});

    const auto pixel = output.at<cv::Vec3b>(0, 0);
    // Linear interpolation at center between 0 and 255 produces 127.5
    // OpenCV cvRound uses round-half-to-even: 127.5 rounds to 128
    CHECK(pixel[0] == 128);
    CHECK(pixel[1] == 128);
    CHECK(pixel[2] == 128);
}

namespace {

// A quad over the middle ninth of the target, which holds only the centre sample of a 3x3 output.
std::array<MappedTriangle, 2> make_centre_quad() {
    const glm::dvec2 low(0.34);
    const glm::dvec2 high(0.66);

    std::array<MappedTriangle, 2> triangles;
    triangles[0].source_uvs = {glm::dvec2(0.5), glm::dvec2(0.5), glm::dvec2(0.5)};
    triangles[0].target_uvs = {low, glm::dvec2(high.x, low.y), high};
    triangles[1].source_uvs = triangles[0].source_uvs;
    triangles[1].target_uvs = {low, high, glm::dvec2(low.x, high.y)};
    return triangles;
}

} // namespace

TEST_CASE("reproject reports the texels its triangles wrote to", "[atlas][texture_reprojection]") {
    const cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(10, 20, 30));
    const std::array<MappedTriangle, 2> triangles = make_centre_quad();

    const RenderedTexture rendered = reproject(
        std::span(&source, 1), triangles, glm::uvec2(3, 3), CV_8UC3, cv::Scalar::all(0),
        ReprojectionOptions{1, texture::Interpolation::Nearest});

    CHECK(rendered.image.at<cv::Vec3b>(1, 1) == cv::Vec3b(10, 20, 30));
    CHECK(rendered.image.at<cv::Vec3b>(0, 0) == cv::Vec3b(0, 0, 0));
    CHECK(rendered.coverage.get(1, 1));
    CHECK_FALSE(rendered.coverage.get(0, 0));
}

TEST_CASE("reproject_into leaves the texels its triangles missed untouched", "[atlas][texture_reprojection]") {
    const cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(10, 20, 30));
    const std::array<MappedTriangle, 2> triangles = make_centre_quad();

    cv::Mat image(3, 3, CV_8UC3, cv::Vec3b(7, 8, 9));
    texture::Mask coverage(glm::uvec2(3, 3));
    reproject_into(
        std::span(&source, 1), triangles, ReprojectionOptions{1, texture::Interpolation::Nearest},
        texture::RenderTarget(image, coverage));

    CHECK(image.at<cv::Vec3b>(1, 1) == cv::Vec3b(10, 20, 30));
    // Never cleared, so an uncovered texel keeps what the caller put there.
    CHECK(image.at<cv::Vec3b>(0, 0) == cv::Vec3b(7, 8, 9));
    CHECK(coverage.get(1, 1));
    CHECK_FALSE(coverage.get(0, 0));
}

TEST_CASE("the gutter dilation extends what the reprojector covered", "[atlas][texture_reprojection]") {
    const cv::Mat source(1, 1, CV_8UC3, cv::Vec3b(10, 20, 30));
    const std::array<MappedTriangle, 2> triangles = make_centre_quad();

    RenderedTexture rendered = reproject(
        std::span(&source, 1), triangles, glm::uvec2(3, 3), CV_8UC3, cv::Scalar::all(0),
        ReprojectionOptions{1, texture::Interpolation::Nearest});

    texture::dilate_colors_inplace(rendered.image, rendered.coverage, 1);

    for (const uint32_t y : range<uint32_t>(3)) {
        for (const uint32_t x : range<uint32_t>(3)) {
            REQUIRE(rendered.image.at<cv::Vec3b>(y, x) == cv::Vec3b(10, 20, 30));
        }
    }
}