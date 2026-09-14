#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "atlas/texture_reprojection.h"
#include "bake/UvGeometry.h"
#include "geometry/PlaneFrame.h"
#include "geometry/geometry.h"
#include "polygon/clip.h"
#include "range_utils.h"
#include "try.h"

namespace bake::detail {

using ClippedTriangle = MappedTriangle;

// A source triangle covering part of the target, projected onto the target's plane.
struct SourceTriangle {
    geometry::Triangle2d projected;
    glm::dvec3 distances; // per corner, from the target surface
    UvTriangle uvs;
    uint32_t image_index = 0;
    bool front_facing = false;
};

// A target triangle projected onto the plane it spans.
struct TargetTriangle {
    PlaneFrame frame;
    geometry::Triangle2d projected;
    UvTriangle uvs;
};

// A clipped triangle, with how far its source sits off the target and which way it faces.
struct OrderedClippedTriangle {
    ClippedTriangle clipped;
    double distance = 0.0;
    bool front_facing = false;

    // Facing away first, then farthest first, so the nearest facing surface is left standing.
    auto operator<=>(const OrderedClippedTriangle &other) const {
        return std::pair(this->front_facing, -this->distance) <=> std::pair(other.front_facing, -other.distance);
    }
};

// Bring one source triangle onto the target's plane.
[[nodiscard]]
inline SourceTriangle project_source(
    const TexturedGeometry &source,
    const uint32_t triangle_index,
    const std::span<const glm::dvec3> positions,
    const PlaneFrame &frame) {
    const glm::uvec3 &triangle = source.geometry.triangles[triangle_index];

    SourceTriangle source_triangle;
    source_triangle.uvs = source.geometry.uvs[triangle_index];
    source_triangle.image_index = source.image_indices[triangle_index];
    for (const uint8_t k : range<uint8_t>(3)) {
        const glm::dvec3 position = positions[triangle[k]];
        source_triangle.projected[k] = frame.project(position);
        source_triangle.distances[k] = frame.distance_to(position);
    }

    const geometry::Triangle3d vertices = geometry::corners(triangle, positions);
    source_triangle.front_facing = glm::dot(radix::geometry::normal(vertices), frame.normal) > 0.0;
    return source_triangle;
}

// Flatten the target triangle into the 2D frame of the plane it spans.
[[nodiscard]]
inline std::optional<TargetTriangle> flatten_target(
    const BakeGeometry &geometry,
    const uint32_t triangle_index) {
    const geometry::Triangle3d vertices =
        geometry::corners(geometry.target.triangles[triangle_index], geometry.positions);
    TRY(const PlaneFrame frame, PlaneFrame::from_triangle(vertices));

    return TargetTriangle{
        .frame = frame,
        .projected = frame.flatten(vertices),
        .uvs = geometry.target.uvs[triangle_index]};
}

// Clips the target against one source and appends each part, described in both uv spaces.
inline void clip_against_source(
    const TargetTriangle &target,
    const SourceTriangle &source,
    std::vector<OrderedClippedTriangle> &ordered_triangles) {
    polygon::clip_into_triangles(target.projected, source.projected, [&](const polygon::Triangle2d &clipped) {
        OrderedClippedTriangle ordered_triangle;
        ordered_triangle.front_facing = source.front_facing;
        ordered_triangle.clipped.source_image_index = source.image_index;
        for (const uint8_t corner : range<uint8_t>(3)) {
            const glm::dvec3 source_weights = geometry::compute_barycentric(clipped[corner], source.projected);
            const glm::dvec3 target_weights = geometry::compute_barycentric(clipped[corner], target.projected);
            ordered_triangle.clipped.source_uvs[corner] = geometry::interpolate(source.uvs, source_weights);
            ordered_triangle.clipped.target_uvs[corner] = geometry::interpolate(target.uvs, target_weights);
        }

        // TODO: Ordering is centroid based and will be wrong for intersecting sources
        const glm::dvec2 centre = (clipped[0] + clipped[1] + clipped[2]) / 3.0;
        ordered_triangle.distance = std::abs(glm::dot(geometry::compute_barycentric(centre, source.projected), source.distances));

        ordered_triangles.push_back(ordered_triangle);
    });
}

// Clips the target against every candidate source and leaves the parts farthest first.
inline void clip_against_candidates(
    const TargetTriangle &target,
    const BakeGeometry &geometry,
    const std::span<const uint32_t> candidates,
    std::vector<OrderedClippedTriangle> &ordered_triangles) {
    ordered_triangles.clear();
    for (const uint32_t candidate : candidates) {
        const SourceTriangle source =
            project_source(geometry.source, candidate, geometry.positions, target.frame);
        clip_against_source(target, source, ordered_triangles);
    }

    std::ranges::stable_sort(ordered_triangles, std::less<>{});
}

} // namespace bake::detail
