#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <ogr_spatialref.h>
#include <radix/geometry.h>
#include <radix/tile.h>

#include "error/Result.h"
#include "error/try.h"
#include "error/utils.h"
#include "srs/transform_points.h"
#include "srs/transformation.h"

namespace srs {

[[nodiscard]]
inline alp::Result<radix::tile::SrsBounds, UntransformablePoint> non_exact_bounds_transform(OGRCoordinateTransformation *transform, const radix::tile::SrsBounds &bounds) {
    std::array xs = {bounds.min.x, bounds.max.x};
    std::array ys = {bounds.min.y, bounds.max.y};
    if (!transform->Transform(2, xs.data(), ys.data())) {
        return alp::make_unexpected(detail::make_untransformable_point(transform));
    }
    return radix::tile::SrsBounds{
        {std::min(xs[0], xs[1]), std::min(ys[0], ys[1])},
        {std::max(xs[0], xs[1]), std::max(ys[0], ys[1])}};
}
[[nodiscard]]
inline alp::Result<radix::tile::SrsBounds, TransformError> non_exact_bounds_transform(const radix::tile::SrsBounds &bounds, const OGRSpatialReference &sourceSrs, const OGRSpatialReference &targetSrs) {
    ALP_TRY(const auto transform, transformation(sourceSrs, targetSrs));
    return non_exact_bounds_transform(transform.get(), bounds);
}
[[nodiscard]]
inline alp::Result<radix::geometry::Aabb3d, UntransformablePoint> non_exact_bounds_transform(OGRCoordinateTransformation *transform, const radix::geometry::Aabb3d &bounds) {
    std::array xs = {bounds.min.x, bounds.max.x};
    std::array ys = {bounds.min.y, bounds.max.y};
    std::array zs = {bounds.min.z, bounds.max.z};
    if (!transform->Transform(2, xs.data(), ys.data(), zs.data())) {
        return alp::make_unexpected(detail::make_untransformable_point(transform));
    }
    return radix::geometry::Aabb3d{{xs[0], ys[0], zs[0]}, {xs[1], ys[1], zs[1]}};
}
[[nodiscard]]
inline alp::Result<radix::geometry::Aabb3d, TransformError> non_exact_bounds_transform(const radix::geometry::Aabb3d &bounds, const OGRSpatialReference &sourceSrs, const OGRSpatialReference &targetSrs) {
    ALP_TRY(const auto transform, transformation(sourceSrs, targetSrs));
    return non_exact_bounds_transform(transform.get(), bounds);
}

/// Transforms bounds from one srs to another,
/// in such a way that all points inside the original bounds are guaranteed to also be in the new bounds.
/// But there can be points inside the new bounds that were not present in the original ones.
[[nodiscard]]
inline alp::Result<radix::tile::SrsBounds, UntransformablePoint> encompassing_bounds_transfer(OGRCoordinateTransformation *transform, const radix::tile::SrsBounds &source_bounds) {
    radix::tile::SrsBounds target_bounds;
    const int result = transform->TransformBounds(
        source_bounds.min.x, source_bounds.min.y, source_bounds.max.x, source_bounds.max.y,
        &target_bounds.min.x, &target_bounds.min.y, &target_bounds.max.x, &target_bounds.max.y,
        21);
    if (result != TRUE) {
        return alp::make_unexpected(detail::make_untransformable_point(transform));
    }
    return target_bounds;
}
[[nodiscard]]
inline alp::Result<radix::tile::SrsBounds, TransformError> encompassing_bounds_transfer(const OGRSpatialReference &source_srs, const OGRSpatialReference &target_srs, const radix::tile::SrsBounds &source_bounds) {
    if (source_srs.IsSame(&target_srs)) {
        return source_bounds;
    }

    ALP_TRY(const std::shared_ptr<OGRCoordinateTransformation> transformation, srs::transformation(source_srs, target_srs));
    return encompassing_bounds_transfer(transformation.get(), source_bounds);
}

[[nodiscard]]
inline alp::Result<radix::geometry::Aabb3d, UntransformablePoint> encompassing_bounds_transfer(
    OGRCoordinateTransformation *transform,
    const radix::geometry::Aabb3d &source_bounds,
    const uint32_t intermediate_points_edges = 21,
    const uint32_t intermediate_points_faces = 5) {
    std::vector<glm::dvec3> points;

    // Add corner points
    const auto corners = radix::geometry::corners(source_bounds);
    std::copy(corners.begin(), corners.end(), std::back_inserter(points));

    // Sample points on the edges
    const auto edges = radix::geometry::edges(source_bounds);
    for (const auto &edge : edges) {
        const auto &[p0, p1] = edge;

        for (uint32_t i = 1; i <= intermediate_points_edges; i++) {
            const double t = static_cast<double>(i) / (intermediate_points_edges + 1);
            const auto p = glm::mix(p0, p1, t);
            points.push_back(p);
        }
    }

    // TODO: do we need this?
    // Sample points on the faces
    const auto quads = radix::geometry::quads(source_bounds);
    for (const auto &quad : quads) {
        const auto &[p0, p1, p2, p3] = quad;

        for (uint32_t i = 1; i <= intermediate_points_faces; i++) {
            const double u = static_cast<double>(i) / (intermediate_points_faces + 1);
            const auto edge_p0 = glm::mix(p0, p1, u);
            const auto edge_p1 = glm::mix(p3, p2, u);

            for (uint32_t j = 1; j <= intermediate_points_faces; j++) {
                const double v = static_cast<double>(j) / (intermediate_points_faces + 1);
                const auto point = glm::mix(edge_p0, edge_p1, v);
                points.push_back(point);
            }
        }
    }

    // Transform all collected points
    ALP_TRY(transform_points_inplace(transform, points));

    // Compute bounds from transformed points
    radix::geometry::Aabb3d target_bounds;
    target_bounds.min = glm::dvec3(std::numeric_limits<double>::max());
    target_bounds.max = glm::dvec3(std::numeric_limits<double>::min());
    for (const auto &point : points) {
        target_bounds.expand_by(point);
    }

    return target_bounds;
}
[[nodiscard]]
inline alp::Result<radix::geometry::Aabb3d, TransformError> encompassing_bounds_transfer(
    const OGRSpatialReference &source_srs,
    const OGRSpatialReference &target_srs,
    const radix::geometry::Aabb3d &source_bounds,
    const uint32_t intermediate_points_edges = 21,
    const uint32_t intermediate_points_faces = 5) {
    if (source_srs.IsSame(&target_srs)) {
        return source_bounds;
    }

    ALP_TRY(const auto transform, srs::transformation(source_srs, target_srs));
    return encompassing_bounds_transfer(transform.get(), source_bounds, intermediate_points_edges, intermediate_points_faces);
}

}
