#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <span>

#include <glm/glm.hpp>
#include <opencv2/core.hpp>
#include <radix/geometry.h>

#include "bake/UvGeometry.h"
#include "enumerate.h"
#include "geometry/geometry.h"
#include "rasterize.h"
#include "texture/sample_image.h"

namespace bake::detail {

// A point located on one of the candidates it was searched among.
struct SourcePoint {
    uint32_t candidate = 0;
    glm::dvec3 barycentric;
    double distance_squared = 0.0;
};

// A source triangle standing under the target, with its orientation against the target's normal.
struct Candidate {
    geometry::Triangle3d triangle;
    double normal_dot = 0.0;
};

// Visits every texel whose centre falls inside the uv triangle, as visit(texel, barycentric).
template <typename Visitor>
void for_each_texel(const UvTriangle &uvs, const glm::uvec2 size, Visitor &&visit) {
    const glm::dvec2 scale(size);
    const radix::geometry::Triangle<2, double> in_texels = {uvs[0] * scale, uvs[1] * scale, uvs[2] * scale};
    const raster::FixedTriangle texel_triangle = raster::make_triangle(in_texels);

    raster::scan_triangle(texel_triangle, size, [&](const glm::uvec2 texel) {
        const glm::dvec2 centre = glm::dvec2(texel) + 0.5;
        visit(texel, geometry::compute_barycentric(centre, in_texels));
    });
}

// The nearest point among the candidates, preferring those facing the query.
[[nodiscard]]
inline std::optional<SourcePoint> find_closest_point(
    const std::span<const Candidate> candidates,
    const glm::dvec3 &point,
    const std::optional<double> max_distance,
    const bool require_facing) {
    const double max_distance_squared = max_distance
        ? max_distance.value() * max_distance.value()
        : std::numeric_limits<double>::infinity();

    std::optional<SourcePoint> nearest_facing_result;
    std::optional<SourcePoint> nearest_any_result;

    for (const auto &[index, candidate] : enumerate<uint32_t>(candidates)) {
        const glm::dvec3 barycentric = geometry::compute_closest_barycentric(point, candidate.triangle);
        const glm::dvec3 offset = point - geometry::interpolate(candidate.triangle, barycentric);
        const double distance_squared = glm::dot(offset, offset);
        if (distance_squared > max_distance_squared) {
            continue;
        }

        const SourcePoint source_point{
            .candidate = index, .barycentric = barycentric, .distance_squared = distance_squared};
        if (!nearest_any_result || distance_squared < nearest_any_result->distance_squared) {
            nearest_any_result = source_point;
        }

        if (candidate.normal_dot > 0.0
            && (!nearest_facing_result || distance_squared < nearest_facing_result->distance_squared)) {
            nearest_facing_result = source_point;
        }
    }

    if (require_facing) {
        return nearest_facing_result;
    }
    return nearest_facing_result ? nearest_facing_result : nearest_any_result;
}

// The nearest candidate the line crosses on either side.
[[nodiscard]]
inline std::optional<SourcePoint> find_line_hit(
    const std::span<const Candidate> candidates,
    const geometry::Line3d &line,
    const double max_distance) {
    const double max_distance_squared = max_distance * max_distance;
    constexpr double epsilon = geometry::epsilon<double>();

    std::optional<SourcePoint> nearest_result;

    for (const auto &[index, candidate] : enumerate<uint32_t>(candidates)) {
        // A source turned away from the target is the far side of a fold, whichever side the line crosses it from.
        if (candidate.normal_dot <= 0.0) {
            continue;
        }

        // Moller-Trumbore, solving the corner weights and the line parameter by Cramer's rule.
        const geometry::Triangle3d &triangle = candidate.triangle;
        const glm::dvec3 first_edge = triangle[1] - triangle[0];
        const glm::dvec3 second_edge = triangle[2] - triangle[0];
        const glm::dvec3 direction_cross_second = glm::cross(line.direction, second_edge);

        // Zero only where the line runs parallel to the triangle, which it then never crosses.
        const double determinant = glm::dot(first_edge, direction_cross_second);
        if (determinant == 0.0) {
            continue;
        }
        const double inverse_determinant = 1.0 / determinant;

        const glm::dvec3 from_corner = line.point - triangle[0];

        glm::dvec3 barycentric;
        barycentric[1] = glm::dot(from_corner, direction_cross_second) * inverse_determinant;
        if (barycentric[1] < -epsilon || barycentric[1] > 1.0 + epsilon) {
            continue;
        }

        const glm::dvec3 from_corner_cross_first = glm::cross(from_corner, first_edge);
        barycentric[2] = glm::dot(line.direction, from_corner_cross_first) * inverse_determinant;

        // Without slack a crossing on a shared edge is dropped by both candidates.
        if (barycentric[2] < -epsilon || barycentric[1] + barycentric[2] > 1.0 + epsilon) {
            continue;
        }
        barycentric[0] = 1.0 - barycentric[1] - barycentric[2];

        // The direction is unit length, so the line parameter is the distance travelled, signed by the side crossed.
        const double distance = glm::dot(second_edge, from_corner_cross_first) * inverse_determinant;
        const double distance_squared = distance * distance;
        if (distance_squared > max_distance_squared) {
            continue;
        }
        if (nearest_result && distance_squared >= nearest_result->distance_squared) {
            continue;
        }

        nearest_result = SourcePoint{
            .candidate = index,
            .barycentric = barycentric,
            .distance_squared = distance_squared};
    }

    return nearest_result;
}

// The colour the source carries at a point on one of its triangles.
template <int channels, typename Value>
[[nodiscard]]
cv::Vec<float, channels> sample_point(
    const TexturedGeometry &source,
    const uint32_t triangle,
    const glm::dvec3 &barycentric,
    const int interpolation) {
    const cv::Mat &image = source.images[source.image_indices[triangle]];
    const glm::dvec2 uv = geometry::interpolate(source.geometry.uvs[triangle], barycentric);

    // Offset by 0.5 to convert from pixel-center to OpenCV texel-index space.
    const glm::dvec2 in_pixels = uv * glm::dvec2(image.cols, image.rows) - glm::dvec2(0.5);
    return texture::sample_image<channels, Value>(image, in_pixels, interpolation);
}

} // namespace bake::detail
