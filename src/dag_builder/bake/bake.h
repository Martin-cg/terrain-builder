#pragma once

#include <cstdint>
#include <numeric>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "bake/UvGeometry.h"
#include "bake/closest_point.h"
#include "bake/interpolate_uncovered.h"
#include "bake/project_along_normal.h"
#include "bake/ray_cast.h"
#include "cluster.h"
#include "correspondence.h"
#include "log.h"
#include "range_utils.h"
#include "texture/clear_color.h"
#include "texture/dilate_colors.h"

namespace bake {

// Options every mode shares.
struct Options {
    CorrespondenceOptions correspondence = {};
    cv::Scalar clear_color = DEFAULT_CLEAR_COLOR;
    uint32_t gutter = 2;
    bool interpolate_uncovered = true;
    bool fill_uncovered = true;
    int interpolation = cv::INTER_LINEAR;
};

// How a bake reads each texel from the source.
using Mode = std::variant<ClosestPoint, RayCast, ProjectAlongNormal>;

namespace detail {

// Collects the index of every cluster in the clustering.
inline std::vector<uint32_t> collect_all_clusters(const Clustering &clustering) {
    std::vector<uint32_t> indices(clustering.cluster_count());
    std::iota(indices.begin(), indices.end(), uint32_t{0});
    return indices;
}

} // namespace detail

// Renders one texture for the target geometry, read back through the source standing under it.
// The destination is cleared first, so this replaces its contents rather than adding to them.
template <typename Mode>
inline void render_texture(
    const BakeGeometry &geometry,
    cv::Mat &destination,
    const Options &options,
    const Mode &mode) {
    destination.setTo(options.clear_color);
    texture::Mask coverage(glm::uvec2(destination.cols, destination.rows));
    const texture::RenderTarget target(destination, coverage);

    const Correspondence correspondence = find_source_triangles(
        geometry.source.geometry.triangles, geometry.target.triangles, geometry.positions, options.correspondence);

    detail::render(geometry, correspondence, target, options.interpolation, mode);

    if (options.interpolate_uncovered) {
        const uint32_t interpolated = interpolate_uncovered(destination, coverage, geometry.target.uvs);
        if (interpolated > 0) {
            LOG_WARN("Interpolated {} texels the bake left uncovered", interpolated);
        }
    }

    texture::dilate_colors_inplace(destination, coverage, options.gutter);
    if (options.fill_uncovered) {
        texture::fill_uncovered_inplace(destination, coverage);
    }
}

// Renders one texture through the given mode.
inline void render_texture(
    const BakeGeometry &geometry,
    cv::Mat &destination,
    const Options &options,
    const Mode &mode) {
    std::visit([&](const auto &m) { render_texture(geometry, destination, options, m); }, mode);
}

// Renders one texture into a new image of the given size, of the type of its sources.
template <typename Mode>
[[nodiscard]]
inline cv::Mat render_texture(
    const BakeGeometry &geometry,
    const glm::uvec2 size,
    const Options &options,
    const Mode &mode) {
    // Match the source type.
    const int type = geometry.source.images.empty() ? CV_8UC3 : geometry.source.images.front().type();
    cv::Mat destination(size.y, size.x, type);
    render_texture(geometry, destination, options, mode);

    return destination;
}

// Renders one texture for a group of target clusters, read back through the given source clusters.
template <typename Mode>
[[nodiscard]]
inline cv::Mat render_texture(
    const ClusteringView target,
    const ClusteringView source,
    const glm::uvec2 size,
    const Options &options,
    const Mode &mode) {
    return render_texture(make_bake_geometry(target, source), size, options, mode);
}

// Renders one texture covering every cluster of both clusterings.
template <typename Mode>
[[nodiscard]]
inline cv::Mat render_texture(
    const Clustering &merged,
    const Clustering &source,
    const glm::uvec2 size,
    const Options &options,
    const Mode &mode) {
    const std::vector<uint32_t> target_clusters = detail::collect_all_clusters(merged);
    const std::vector<uint32_t> source_clusters = detail::collect_all_clusters(source);
    return render_texture(
        ClusteringView{merged, target_clusters},
        ClusteringView{source, source_clusters},
        size, options, mode);
}

} // namespace bake