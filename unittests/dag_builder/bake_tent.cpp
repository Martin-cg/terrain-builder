#include <cstdint>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "../catch2_helpers.h"

#include "bake/bake.h"
#include "cluster.h"
#include "enumerate.h"
#include "range_utils.h"

namespace {

// A tent of two 3-4-5 faces, and the bulged version of it that parts from it between the vertices
// they share.
struct Tent {
    Clustering faces;
    Clustering bulged;
};

constexpr glm::uvec2 target_size{4, 1};
constexpr uint32_t texture_size = 180;

// A ramp along x, so a texel reports the index it was read at.
cv::Mat make_ramp_texture() {
    cv::Mat image(1, texture_size, CV_8UC3);
    for (const uint32_t x : range(texture_size)) {
        image.at<cv::Vec3b>(0, x) = cv::Vec3b(60, 120, static_cast<uint8_t>(x));
    }
    return image;
}

// The faces are 3-4-5, so their normals are rational unit vectors and a hit against the bulged
// surface lands on a rational point. The bulge lifts it above the tent on the left face and drops it
// below on the right.
Tent make_tent(const double bulge) {
    const std::vector<glm::dvec3> positions = {
        {0, 0, 0}, {0, 1, 0},
        {3, 0, 4}, {3, 1, 4},
        {6, 0, 0}, {6, 1, 0},
        {1.5, 0, 2 + bulge}, {1.5, 1, 2 + bulge},
        {4.5, 0, 2 - bulge}, {4.5, 1, 2 - bulge}};

    Cluster faces;
    faces.vertex_indices = {0, 1, 2, 3, 4, 5};
    // Diagonals mirrored about the crease, so its vertices carry an exactly vertical normal.
    faces.local_triangles = {{0, 2, 3}, {0, 3, 1}, {4, 3, 2}, {4, 5, 3}};
    faces.uvs = {{1.0, 0.0}, {1.0, 1.0}, {0.5, 0.0}, {0.5, 1.0}, {0.0, 0.0}, {0.0, 1.0}};

    Cluster bulged;
    bulged.vertex_indices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    bulged.local_triangles = {
        {0, 6, 7}, {0, 7, 1}, {6, 2, 3}, {6, 3, 7},
        {2, 8, 9}, {2, 9, 3}, {8, 4, 5}, {8, 5, 9}};
    bulged.uvs = {
        {0.0, 0.0}, {0.0, 1.0}, {0.5, 0.0}, {0.5, 1.0}, {1.0, 0.0}, {1.0, 1.0},
        {0.25, 0.0}, {0.25, 1.0}, {0.75, 0.0}, {0.75, 1.0}};
    bulged.texture_id = 0;

    Tent tent;
    tent.faces.positions = positions;
    tent.faces.clusters = {std::move(faces)};
    tent.bulged.positions = positions;
    tent.bulged.clusters = {std::move(bulged)};
    tent.bulged.textures.add(make_ramp_texture());
    return tent;
}

// One source texel a target texel, so a colour names the texel the mode read, and nothing fills a
// texel the bake itself did not write.
const bake::Options options{
    .clear_color = cv::Scalar(0, 0, 0), .gutter = 0, .interpolate_uncovered = false, .fill_uncovered = false, .interpolation = cv::INTER_NEAREST};

// The colour a texel keeps when no mode wrote it, which the ramp itself never carries.
const cv::Vec3b uncovered(0, 0, 0);

void check_texels(const cv::Mat &image, const std::vector<cv::Vec3b> &expected) {
    REQUIRE(image.rows == 1);
    REQUIRE(image.total() == expected.size());

    for (const auto &[x, color] : enumerate<uint32_t>(expected)) {
        CHECK(image.at<cv::Vec3b>(0, x) == color);
    }
}

} // namespace

TEST_CASE("project along normal reads the source along the target's face normal", "[dag_builder][bake]") {
    const Tent tent = make_tent(2.5);

    const cv::Mat rendered = bake::render_texture(
        tent.faces, tent.bulged, target_size, options,
        bake::ProjectAlongNormal{.sample_scale = 1});

    // The face normal is constant at 3-4-5 across each face, reaching x = 4.25, 3.417, 1.25, 0.417.
    check_texels(rendered, {{60, 120, 127}, {60, 120, 102}, {60, 120, 37}, {60, 120, 12}});
}
