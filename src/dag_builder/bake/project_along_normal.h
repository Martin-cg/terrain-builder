#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "atlas/texture_reprojection.h"
#include "bake/UvGeometry.h"
#include "bake/flatten.h"
#include "correspondence.h"
#include "log.h"
#include "texture/Interpolation.h"

namespace bake {

// Flatten each target triangle onto its own plane and clip its sources onto it.
struct ProjectAlongNormal {
    uint32_t sample_scale = 2;
};

namespace detail {

// Clips every target triangle against the sources standing under it, farthest part first.
[[nodiscard]]
inline std::vector<ClippedTriangle> clip_all_triangles(
    const BakeGeometry &geometry,
    const Correspondence &correspondence) {
    DEBUG_ASSERT(correspondence.segment_count() == geometry.target.triangles.size());

    std::vector<ClippedTriangle> clipped_triangles;
    clipped_triangles.reserve(geometry.target.triangles.size());
    std::vector<OrderedClippedTriangle> ordered_triangles;
    uint32_t without_mappings = 0;

    for (const uint32_t triangle_index : range<uint32_t>(geometry.target.triangles.size())) {
        const std::optional<TargetTriangle> target = flatten_target(geometry, triangle_index);
        if (!target) {
            without_mappings++;
            continue;
        }

        clip_against_candidates(*target, geometry, correspondence.segment(triangle_index), ordered_triangles);
        without_mappings += ordered_triangles.empty() ? 1 : 0;

        for (const OrderedClippedTriangle &ordered_triangle : ordered_triangles) {
            clipped_triangles.push_back(ordered_triangle.clipped);
        }
    }

    if (without_mappings > 0) {
        // Nothing is rendered for these, so their texels keep the colour the bake cleared to.
        LOG_WARN("{} of {} target triangles produced no mapping to render", without_mappings, geometry.target.triangles.size());
    }

    return clipped_triangles;
}

inline void render(
    const BakeGeometry &geometry,
    const Correspondence &correspondence,
    const texture::RenderTarget target,
    const texture::Interpolation interpolation,
    const ProjectAlongNormal &mode) {
    const std::vector<ClippedTriangle> clipped_triangles = clip_all_triangles(geometry, correspondence);

    reproject_into(
        geometry.source.images, clipped_triangles,
        ReprojectionOptions{mode.sample_scale, interpolation}, target);
}

} // namespace detail

} // namespace bake