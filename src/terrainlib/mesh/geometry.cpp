#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/geometry.h"
#include "optional_utils.h"
#include "range_utils.h"

namespace mesh {

std::vector<glm::dvec3> compute_vertex_normals(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::dvec3> positions) {
    std::vector<glm::dvec3> normals(positions.size(), glm::dvec3(0));

    // Simplification leaves wildly uneven triangle sizes behind, so weighting by the corner angle
    // keeps a fan of small triangles from outvoting one large neighbour.
    for (const glm::uvec3 &triangle : triangles) {
        const geometry::Triangle3d vertices = geometry::corners(triangle, positions);
        const glm::dvec3 normal = geometry::compute_normal(vertices[0], vertices[1], vertices[2]);
        if (glm::length2(normal) == 0.0) {
            continue;
        }

        const glm::dvec3 angles = geometry::compute_triangle_angles(vertices);
        for (const uint8_t k : range<uint8_t>(3)) {
            normals[triangle[k]] += normal * angles[k];
        }
    }

    for (glm::dvec3 &normal : normals) {
        if (glm::length2(normal) > 0.0) {
            normal = glm::normalize(normal);
        }
    }

    return normals;
}
    
std::optional<double> estimate_average_edge_length(const SimpleMesh &mesh, const size_t sample_size) {
    const auto &triangles = mesh.triangles;
    const auto &positions = mesh.positions;
    const size_t num_triangles = triangles.size();

    if (num_triangles == 0) {
        return std::nullopt;
    }

    const size_t triangle_sample_size = std::min((sample_size + 2) / 3, mesh.face_count());
    const size_t stride = std::max<size_t>(1, num_triangles / triangle_sample_size);

    double total_length = 0.0;

    // Use a small offset to avoid sampling only the first part of the mesh
    const size_t offset = (num_triangles / 7) % num_triangles;

    for (size_t i = 0; i < triangle_sample_size; i++) {
        const auto &triangle = triangles[(offset + i * stride) % num_triangles];

        const glm::dvec3 &a = positions[triangle.x];
        const glm::dvec3 &b = positions[triangle.y];
        const glm::dvec3 &c = positions[triangle.z];

        const double ab = glm::distance(a, b);
        const double bc = glm::distance(b, c);
        const double ca = glm::distance(c, a);

        total_length += ab + bc + ca;
    }

    return total_length / (triangle_sample_size * 3);
}

std::optional<double> calculate_max_edge_length_squared(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double max_length_sq = 0.0;
    for (const auto &triangle : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[triangle.x];
        const glm::dvec3 &b = mesh.positions[triangle.y];
        const glm::dvec3 &c = mesh.positions[triangle.z];

        const double ab_sq = glm::distance2(a, b);
        const double bc_sq = glm::distance2(b, c);
        const double ca_sq = glm::distance2(c, a);

        max_length_sq = std::max({
            max_length_sq,
            ab_sq,
            bc_sq,
            ca_sq,
        });
    }
    return max_length_sq;
}

std::optional<double> calculate_min_edge_length_squared(const SimpleMesh &mesh) {
    if (mesh.face_count() == 0) {
        return std::nullopt;
    }

    double min_length_sq = std::numeric_limits<double>::max();
    for (const auto &triangle : mesh.triangles) {
        const glm::dvec3 &a = mesh.positions[triangle.x];
        const glm::dvec3 &b = mesh.positions[triangle.y];
        const glm::dvec3 &c = mesh.positions[triangle.z];

        const double ab_sq = glm::distance2(a, b);
        const double bc_sq = glm::distance2(b, c);
        const double ca_sq = glm::distance2(c, a);

        min_length_sq = std::min({
            min_length_sq,
            ab_sq,
            bc_sq,
            ca_sq,
        });
    }
    return min_length_sq;
}

std::optional<double> calculate_max_edge_length(const SimpleMesh &mesh) {
    return map(calculate_max_edge_length_squared(mesh), [](const double max_len_sq) { return std::sqrt(max_len_sq); });
}

std::optional<double> calculate_min_edge_length(const SimpleMesh &mesh) {
    return map(calculate_min_edge_length_squared(mesh), [](const double min_len_sq) { return std::sqrt(min_len_sq); });
}

}
