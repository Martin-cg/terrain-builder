#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <radix/geometry.h>

#include "bake/UvGeometry.h"
#include "bake/query.h"
#include "correspondence.h"
#include "geometry/geometry.h"
#include "opencv_utils.h"
#include "range_utils.h"
#include "texture/RenderTarget.h"

namespace bake::detail {

// What a sampler is asked: which source a point on a target triangle reads.
struct PointQuery {
    std::span<const Candidate> candidates; // in correspondence order
    glm::dvec3 point; // the point being shaded
    glm::dvec3 barycentric; // that point, in corner weights of the target triangle
    uint32_t target_index = 0;
};

// Shades every texel the target covers, from the source the sampler picks for it.
template <int channels, typename Value, typename Sampler>
void render_point_texels(
    const BakeGeometry &geometry,
    const Correspondence &correspondence,
    const texture::RenderTarget target,
    const int interpolation,
    const Sampler &sampler) {
    // Searched once per texel, so they are gathered once per target triangle.
    std::vector<Candidate> candidates;

    for (const uint32_t target_index : range<uint32_t>(geometry.target.triangles.size())) {
        const std::span<const uint32_t> correspondents = correspondence.segment(target_index);
        if (correspondents.empty()) {
            continue;
        }

        const geometry::Triangle3d target_triangle =
            geometry::corners(geometry.target.triangles[target_index], geometry.positions);
        const glm::dvec3 normal = geometry::compute_normal(target_triangle);

        // The orientation of a source against the target does not vary over the texels, so it is measured here.
        candidates.clear();
        for (const uint32_t correspondent : correspondents) {
            const geometry::Triangle3d triangle =
                geometry::corners(geometry.source.geometry.triangles[correspondent], geometry.positions);
            candidates.push_back(Candidate{
                .triangle = triangle,
                .normal_dot = glm::dot(geometry::compute_normal(triangle), normal)});
        }

        for_each_texel(geometry.target.uvs[target_index], target.size(), [&](const glm::uvec2 texel, const glm::dvec3 &barycentric) {
            const PointQuery query{
                .candidates = candidates,
                .point = geometry::interpolate(target_triangle, barycentric),
                .barycentric = barycentric,
                .target_index = target_index};

            const std::optional<SourcePoint> hit = sampler(query);
            if (!hit) {
                return;
            }

            // A sampler answers in candidate order, which the correspondence maps back to a source.
            const cv::Vec<float, channels> color = sample_point<channels, Value>(
                geometry.source, correspondents[hit->candidate], hit->barycentric, interpolation);
            target.image.at<cv::Vec<Value, channels>>(texel.y, texel.x) = convert_texel<Value>(color);
            target.coverage.set(texel.x, texel.y);
        });
    }
}

// Shades every texel the target covers, from sources of the target's type.
template <typename Sampler>
void render_points(
    const BakeGeometry &geometry,
    const Correspondence &correspondence,
    const texture::RenderTarget target,
    const int interpolation,
    const Sampler &sampler) {
    ASSERT(std::ranges::all_of(geometry.source.images, [&](const cv::Mat &m) { return m.type() == target.image.type(); }));
    DEBUG_ASSERT(correspondence.segment_count() == geometry.target.triangles.size());

    visit_by_depth_and_channels(target.image.type(), [&]<int channels, typename Value>() {
        render_point_texels<channels, Value>(geometry, correspondence, target, interpolation, sampler);
    });
}

} // namespace bake::detail
