#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "geometry/PlaneFrame.h"
#include "atlas/BakeSource.h"
#include "atlas/texture_reprojection.h"
#include "correspondence.h"
#include "enumerate.h"
#include "geometry/geometry.h"
#include "log.h"
#include "polygon/clip.h"
#include "range_utils.h"

namespace detail {

// An output triangle on its own plane, together with the atlas patch it was given.
struct FlatTarget {
    PlaneFrame frame;
    radix::geometry::Triangle<2, double> projected;
    std::array<glm::dvec2, 3> uvs;
};

// A source triangle on the target's plane, with the height it sits at.
struct FlatSource {
    radix::geometry::Triangle<2, double> projected;
    glm::dvec3 heights; // per corner, off the target plane
    std::array<glm::dvec2, 3> uvs;
    uint32_t map_index = 0;
    bool front_facing = false;
};

// A part of an output triangle lying under exactly one source triangle.
struct Fragment {
    double height = 0.0;
    bool front_facing = false;
    MappedTriangle triangle;

    // Facing away first, then farthest first, so the nearest facing surface is left standing.
    auto operator<=>(const Fragment &other) const {
        return std::pair(this->front_facing, -this->height) <=> std::pair(other.front_facing, -other.height);
    }
};

// Empty for a degenerate output triangle, which spans no plane to measure against.
inline std::optional<FlatTarget> flatten_target(
    const glm::uvec3 &triangle,
    const std::span<const glm::dvec2> uvs,
    const std::span<const glm::dvec3> positions) {
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(triangle, positions);
    if (!frame) {
        return std::nullopt;
    }
    return FlatTarget{
        .frame = *frame,
        .projected = frame->flatten(triangle, positions),
        .uvs = {uvs[triangle.x], uvs[triangle.y], uvs[triangle.z]},
    };
}

inline FlatSource flatten_source(
    const BakeSource &source,
    const uint32_t triangle_index,
    const PlaneFrame &frame) {
    const glm::uvec3 triangle = source.triangles[triangle_index];
    const UvRef &mapping = source.uv_triangles[triangle_index];
    const std::vector<glm::dvec2> &map_uvs = source.uv_maps[mapping.map_index];

    FlatSource flat;
    flat.map_index = mapping.map_index;
    for (const uint8_t corner : range<uint8_t>(3)) {
        const glm::dvec3 position = source.positions[triangle[corner]];
        flat.projected[corner] = frame.project(position);
        flat.heights[corner] = frame.distance_to(position);
        flat.uvs[corner] = map_uvs[mapping.uvs[corner]];
    }
    const radix::geometry::Triangle<3, double> corners = geometry::corners(triangle, source.positions);
    flat.front_facing = glm::dot(geometry::compute_normal(corners), frame.normal) > 0.0;
    return flat;
}

// Cuts the output triangle against one source triangle, and describes each part in both uv spaces.
inline void clip_against_source(
    const FlatTarget &target,
    const FlatSource &source,
    std::vector<polygon::Triangle2d> &overlap,
    std::vector<Fragment> &fragments) {
    overlap.clear();
    polygon::clip_into_triangles(target.projected, source.projected, overlap);

    for (const polygon::Triangle2d &part : overlap) {
        Fragment fragment;
        fragment.front_facing = source.front_facing;
        fragment.triangle.source_image_index = source.map_index;
        for (const uint8_t corner : range<uint8_t>(3)) {
            const glm::dvec3 source_weights = geometry::compute_barycentric(part[corner], source.projected);
            const glm::dvec3 target_weights = geometry::compute_barycentric(part[corner], target.projected);
            fragment.triangle.source_uvs[corner] = geometry::interpolate(source.uvs, source_weights);
            fragment.triangle.target_uvs[corner] = geometry::interpolate(target.uvs, target_weights);
        }

        // TODO: Sort is centroid based and will be wrong for intersecting parts
        const glm::dvec2 centre = (part[0] + part[1] + part[2]) / 3.0;
        fragment.height = std::abs(glm::dot(geometry::compute_barycentric(centre, source.projected), source.heights));

        fragments.push_back(fragment);
    }
}

} // namespace detail

// Prepare the triangles for the bake based on the correspondances and the target uv layout.
[[nodiscard]]
inline std::vector<MappedTriangle> build_reprojection_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec2> uvs,
    const std::span<const glm::dvec3> positions,
    const Correspondence &correspondence,
    const BakeSource &source) {
    DEBUG_ASSERT(correspondence.segment_count() == triangles.size());

    std::vector<MappedTriangle> reprojection;
    // At least one fragment per output triangle, more where sources overlap it.
    reprojection.reserve(triangles.size());
    std::vector<detail::Fragment> fragments;
    std::vector<polygon::Triangle2d> overlap;
    uint32_t without_fragments = 0;

    for (const auto [triangle_index, triangle] : enumerate(triangles)) {
        const std::optional<detail::FlatTarget> target = detail::flatten_target(triangle, uvs, positions);
        if (!target) {
            without_fragments++;
            continue;
        }

        fragments.clear();
        for (const uint32_t source_index : correspondence.segment(triangle_index)) {
            const detail::FlatSource flat = detail::flatten_source(source, source_index, target->frame);
            detail::clip_against_source(*target, flat, overlap, fragments);
        }

        without_fragments += fragments.empty() ? 1 : 0;

        // Facing away first, then farthest first, so the nearest facing surface is the one left standing where they overlap.
        std::ranges::stable_sort(fragments, std::less<>{});
        for (const detail::Fragment &fragment : fragments) {
            reprojection.push_back(fragment.triangle);
        }
    }

    if (without_fragments > 0) {
        // Nothing will be rendered for these, so their texels keep the colour the bake cleared to.
        LOG_WARN("{} of {} output triangles produced no fragment to render", without_fragments, triangles.size());
    }

    return reprojection;
}