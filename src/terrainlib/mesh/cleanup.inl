#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/geometry.h"
#include "mesh/normalize.h"
#include "mesh/connectivity/adjacency.h"

namespace mesh {

template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::Simple_<3, T> &mesh, bool ignore_orientation) {
    return find_duplicate_triangles<T>(mesh.triangles, mesh.positions, ignore_orientation);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::View_<3, T> &mesh, bool ignore_orientation) {
    return find_duplicate_triangles<T>(mesh.triangles, mesh.positions, ignore_orientation);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    const bool ignore_orientation) {
    return find_duplicate_triangles(std::span(triangles), std::span(positions), ignore_orientation);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    bool ignore_orientation) {
    if (ignore_orientation) {
        return find_duplicate_triangles_ignore_orientation(triangles, positions);
    } else {
        return find_duplicate_triangles_consider_orientation(triangles);
    }
}

template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(const mesh::Simple_<3, T> &mesh) {
    return find_duplicate_triangles_ignore_orientation(mesh.triangles, mesh.positions);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(const mesh::View_<3, T> &mesh) {
    return find_duplicate_triangles_ignore_orientation(mesh.triangles, mesh.positions);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions) {
    return find_duplicate_triangles_ignore_orientation(std::span(triangles), std::span(positions));
}

template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const mesh::Simple_<3, T> &mesh) {
    return find_duplicate_triangles_consider_orientation(mesh.triangles);
}
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const mesh::View_<3, T> &mesh) {
    return find_duplicate_triangles_consider_orientation(mesh.triangles);
}
inline std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::vector<glm::uvec3> &triangles) {
    return find_duplicate_triangles_consider_orientation(std::span(triangles));
}

template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions) {
    const SegmentedBuffer<uint32_t, uint32_t> groups = find_duplicate_triangle_groups(triangles, true);

    std::vector<uint32_t> representatives;
    representatives.reserve(groups.segment_count());
    for (uint32_t group_index = 0; group_index < groups.segment_count(); group_index++) {
        representatives.push_back(groups.segment(group_index).front());
    }
    const SegmentedBuffer<uint32_t, uint32_t> neighbourhoods = find_edge_neighbourhoods(triangles, representatives);

    std::vector<uint32_t> duplicates;
    for (uint32_t group_index = 0; group_index < groups.segment_count(); group_index++) {
        const std::span<const uint32_t> group = groups.segment(group_index);
        const glm::uvec3 first = triangles[group.front()];

        // Only the sign matters, so the area-weighted neighbourhood normal is never normalized.
        const glm::vec<3, T> first_normal = geometry::compute_normal(first, positions, false);
        T alignment = 0;
        for (const uint32_t neighbour : neighbourhoods.segment(group_index)) {
            alignment += glm::dot(first_normal, geometry::compute_normal(triangles[neighbour], positions, false));
        }

        // Copies share their vertices, so each faces either along the first or against it.
        uint32_t kept = group.front();
        if (alignment < 0) {
            const glm::uvec3 first_rotation = normalize_triangle(first);
            const auto flipped = std::ranges::find_if(group, [&](const uint32_t t) { return normalize_triangle(triangles[t]) != first_rotation; });
            if (flipped != group.end()) {
                kept = *flipped;
            }
        }

        for (const uint32_t triangle_index : group) {
            if (triangle_index != kept) {
                duplicates.push_back(triangle_index);
            }
        }
    }
    std::ranges::sort(duplicates);

    return duplicates;
}

template <typename T>
void remove_duplicate_triangles(mesh::Simple_<3, T> &mesh, bool ignore_orientation) {
    remove_duplicate_triangles<T>(mesh.triangles, mesh.positions, ignore_orientation);
}
template <typename T>
void remove_duplicate_triangles(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions,
    const bool ignore_orientation) {
    const std::vector<uint32_t> triangles_to_remove = find_duplicate_triangles(triangles, positions, ignore_orientation);

    // Ensure indices are sorted in ascending order
    DEBUG_ASSERT(std::is_sorted(triangles_to_remove.begin(), triangles_to_remove.end()) && "triangles_to_remove must be sorted!");

    // Remove duplicates in reverse order
    for (auto it = triangles_to_remove.rbegin(); it != triangles_to_remove.rend(); ++it) {
        triangles.erase(triangles.begin() + *it);
    }
}
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    const bool ignore_orientation) {
    remove_duplicate_triangles<T>(triangles, std::span<const glm::vec<3, T>>(positions), ignore_orientation);
}
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions) {
    remove_duplicate_triangles<T>(triangles, positions, true);
}
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions) {
    remove_duplicate_triangles<T>(triangles, std::span<const glm::vec<3, T>>(positions));
}
template <typename T>
void remove_duplicate_triangles_consider_orientation(mesh::Simple_<3, T> &mesh) {
    remove_duplicate_triangles_consider_orientation(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
size_t remove_isolated_vertices(mesh::Simple_<n_dims, T> &mesh) {
    const bool has_uvs = mesh.has_uvs();
    const std::vector<uint32_t> isolated = find_isolated_vertices(mesh);
    DEBUG_ASSERT(std::is_sorted(isolated.begin(), isolated.end()));

    for (uint32_t i : isolated | std::views::reverse) {
        const uint32_t last_index = static_cast<uint32_t>(mesh.positions.size() - 1);

        if (i != last_index) {
            std::swap(mesh.positions[i], mesh.positions[last_index]);
            if (has_uvs) {
                std::swap(mesh.uvs[i], mesh.uvs[last_index]);
            }

            for (glm::uvec3 &triangle : mesh.triangles) {
                change_vertex_inplace(triangle, last_index, i);
            }
        }

        mesh.positions.pop_back();
        if (has_uvs) {
            mesh.uvs.pop_back();
        }
    }

    return isolated.size();
}

template <glm::length_t n_dims, typename T>
size_t remove_triangles_of_negligible_size(
    mesh::Simple_<n_dims, T> &mesh,
    const T threshold_percentage_of_average) {
    const size_t triangle_count = mesh.triangles.size();

    std::vector<T> areas;
    areas.reserve(triangle_count);
    for (const glm::uvec3 &triangle : mesh.triangles) {
        const T area = geometry::compute_triangle_area(triangle, mesh.positions);
        areas.push_back(area);
    }

    const T total_area = std::reduce(areas.begin(), areas.end(), static_cast<T>(0));
    const T average_area = total_area / static_cast<T>(areas.size());

    const size_t erased_count = std::erase_if(mesh.triangles,
        [&](const glm::uvec3 &tri) {
            const size_t idx = &tri - &mesh.triangles.front();
            return areas[idx] < average_area * threshold_percentage_of_average;
        });
    return erased_count;
}

}
