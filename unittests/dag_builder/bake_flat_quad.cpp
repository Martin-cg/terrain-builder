#include <cmath>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/opencv.hpp>

#include "../catch2_helpers.h"

#include "bake/bake.h"
#include "cluster.h"
#include "range_utils.h"

namespace {

// A regular grid over the unit square in the xy plane, partitioned into one cluster per half.
struct Grid {
    std::vector<glm::dvec3> positions;
    std::vector<Cluster> clusters;
    glm::uvec4 corners = {};
};

constexpr glm::uvec2 target_size{4, 4};

// An odd multiple of the target size, so a target texel centre lands on a source texel centre with a
// texel to spare at either end for a supersampled bake to read.
constexpr uint32_t source_texture_size = 3 * target_size.x;

Grid make_grid(const uint32_t divisions) {
    const uint32_t side = divisions + 1;

    Grid grid;
    grid.positions.reserve(side * side);
    for (const uint32_t row : range(side)) {
        for (const uint32_t column : range(side)) {
            grid.positions.push_back(
                {static_cast<double>(column) / divisions, static_cast<double>(row) / divisions, 0.0});
        }
    }

    const uint32_t columns = divisions / 2 + 1;
    for (const uint32_t half : range<uint32_t>(2)) {
        const uint32_t first_column = half * (divisions / 2);

        Cluster cluster;
        cluster.vertex_indices.reserve(side * columns);
        for (const uint32_t row : range(side)) {
            for (const uint32_t column : range(columns)) {
                cluster.vertex_indices.push_back(row * side + first_column + column);
            }
        }

        for (const uint32_t row : range(side - 1)) {
            for (const uint32_t column : range(columns - 1)) {
                const uint32_t lower_left = row * columns + column;
                const uint32_t upper_left = lower_left + columns;
                cluster.local_triangles.push_back({lower_left, lower_left + 1, upper_left + 1});
                cluster.local_triangles.push_back({lower_left, upper_left + 1, upper_left});
            }
        }

        for (const uint32_t row : range(side)) {
            for (const uint32_t column : range(columns)) {
                cluster.uvs.push_back(
                    {static_cast<double>(first_column + column) / divisions,
                     static_cast<double>(row) / divisions});
            }
        }

        cluster.texture_id = half;
        grid.clusters.push_back(std::move(cluster));
    }

    grid.corners = {0, divisions, side * side - 1, side * (side - 1)};
    return grid;
}

// Two triangles over four corner vertices, with uvs mirrored in u.
Cluster make_quad(const glm::uvec4 corners) {
    Cluster quad;
    quad.vertex_indices = {corners[0], corners[1], corners[2], corners[3]};
    quad.local_triangles = {{0, 1, 2}, {0, 2, 3}};
    quad.uvs = {{1.0, 0.0}, {0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}};
    return quad;
}

// Rising with x and y, so the mean over a texel's footprint equals the value at its centre and a
// bake reads the same colour however it filters. The first channel stays constant.
cv::Mat make_gradient_texture(const uint32_t size, const uint8_t constant) {
    cv::Mat image(size, size, CV_8UC3);
    for (const uint32_t y : range(size)) {
        for (const uint32_t x : range(size)) {
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(
                constant, static_cast<uint8_t>(2 * y + 10), static_cast<uint8_t>(2 * x + 10));
        }
    }
    return image;
}

} // namespace

TEMPLATE_TEST_CASE(
    "a bake reads a flat source back through the target's own uvs",
    "[dag_builder][bake]",
    bake::ProjectAlongNormal,
    bake::RayCast,
    bake::ClosestPoint) {
    const Grid grid = make_grid(2);

    Clustering source;
    source.positions = grid.positions;
    source.clusters = grid.clusters;
    source.textures.add(make_gradient_texture(source_texture_size, 40));
    source.textures.add(make_gradient_texture(source_texture_size, 90));

    Clustering target;
    target.positions = grid.positions;
    target.clusters = {make_quad(grid.corners)};

    // Nothing may fill a texel the bake itself did not write, so a hole cannot pass as a colour.
    const bake::Options options{.gutter = 0, .interpolate_uncovered = false, .fill_uncovered = false};

    const cv::Mat rendered = bake::render_texture(target, source, target_size, options, TestType{});

    for (const uint32_t ty : range<uint32_t>(target_size.y)) {
        for (const uint32_t tx : range<uint32_t>(target_size.x)) {
            // The target mirrors u, so its texel centre reads the source at x = 1 - (tx + 0.5) / 4, while its v passes straight through. The source uvs are the grid coordinates, so that position names one source texel.
            const double x = 1.0 - (tx + 0.5) / target_size.x;
            const double y = (ty + 0.5) / target_size.y;
            const uint32_t column = static_cast<uint32_t>(std::lround(x * source_texture_size - 0.5));
            const uint32_t row = static_cast<uint32_t>(std::lround(y * source_texture_size - 0.5));

            const cv::Mat &texture = source.textures[x > 0.5 ? 1 : 0];
            REQUIRE(rendered.at<cv::Vec3b>(ty, tx) == texture.at<cv::Vec3b>(row, column));
        }
    }
}