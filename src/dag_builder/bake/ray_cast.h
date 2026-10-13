#pragma once

#include <limits>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "bake/UvGeometry.h"
#include "bake/query.h"
#include "bake/render_points.h"
#include "correspondence.h"
#include "geometry/geometry.h"
#include "mesh/geometry.h"
#include "texture/Interpolation.h"
#include "texture/RenderTarget.h"

namespace bake {

// Take each texel from the first source hit along a cast ray.
struct RayCast {
    std::optional<double> max_distance = {};
};

namespace detail {

inline void render(
    const BakeGeometry &geometry,
    const Correspondence &correspondence,
    const texture::RenderTarget target,
    const texture::Interpolation interpolation,
    const RayCast &mode) {
    // TODO: Only this bake's triangles contribute, so two bakes sharing a vertex cast it along different normals; prepare them once per target clustering instead.
    // Indexed by global vertex, so a triangle's corner normals are its corner indices.
    const std::vector<glm::dvec3> normals =
        mesh::compute_vertex_normals(geometry.target.triangles, geometry.positions);

    const double max_distance = mode.max_distance.value_or(std::numeric_limits<double>::infinity());

    render_points(geometry, correspondence, target, interpolation, [&](const PointQuery &query) {
        const glm::uvec3 &triangle = geometry.target.triangles[query.target_index];
        const glm::dvec3 interpolated_normal = geometry::interpolate(
            geometry::Triangle3d{normals[triangle[0]], normals[triangle[1]], normals[triangle[2]]},
            query.barycentric);
        if (glm::length2(interpolated_normal) == 0.0) {
            return std::optional<SourcePoint>{};
        }

        // The source can lie on either side, so the whole line is searched rather than a ray each way.
        return find_line_hit(
            query.candidates, {query.point, glm::normalize(interpolated_normal)}, max_distance);
    });
}

} // namespace detail

} // namespace bake
