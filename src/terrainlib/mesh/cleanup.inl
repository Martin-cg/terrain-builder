#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <ranges>
#include <span>
#include <tuple>
#include <type_traits>
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
void remove_duplicate_triangles(
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
    remove_duplicate_triangles<T>(triangles, std::span<const glm::vec<3, T>>(positions), true);
}
template <typename T>
void remove_duplicate_triangles_consider_orientation(mesh::Simple_<3, T> &mesh) {
    remove_duplicate_triangles_consider_orientation(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
void remove_unused_vertices(mesh::Simple_<n_dims, T> &mesh, const std::span<const uint32_t> sorted_vertices) {
    DEBUG_ASSERT(std::ranges::is_sorted(sorted_vertices));
    if (sorted_vertices.empty()) {
        return;
    }

    // Kept vertices are numbered in their original order; removed ones map to nothing.
    constexpr uint32_t no_vertex = std::numeric_limits<uint32_t>::max();
    const uint32_t vertex_count = static_cast<uint32_t>(mesh.vertex_count());
    std::vector<uint32_t> map(vertex_count);
    uint32_t next_removed = 0;
    uint32_t next_index = 0;
    for (uint32_t old_index = 0; old_index < vertex_count; old_index++) {
        if (next_removed < sorted_vertices.size() && sorted_vertices[next_removed] == old_index) {
            map[old_index] = no_vertex;
            next_removed++;
        } else {
            map[old_index] = next_index;
            next_index++;
        }
    }
    const uint32_t new_vertex_count = next_index;

    // Kept vertices only move down, so a forward pass never overwrites one it has yet to read.
    const bool has_uvs = mesh.has_uvs();
    for (uint32_t old_index = 0; old_index < vertex_count; old_index++) {
        const uint32_t new_index = map[old_index];
        if (new_index == no_vertex) {
            continue;
        }
        mesh.positions[new_index] = mesh.positions[old_index];
        if (has_uvs) {
            mesh.uvs[new_index] = mesh.uvs[old_index];
        }
    }
    mesh.positions.resize(new_vertex_count);
    if (has_uvs) {
        mesh.uvs.resize(new_vertex_count);
    }

    for (glm::uvec3 &triangle : mesh.triangles) {
        triangle = glm::uvec3(map[triangle.x], map[triangle.y], map[triangle.z]);
        DEBUG_ASSERT(triangle.x != no_vertex && triangle.y != no_vertex && triangle.z != no_vertex, "A removed vertex is still used by a triangle");
    }
}

template <glm::length_t n_dims, typename T>
size_t remove_unused_vertices(mesh::Simple_<n_dims, T> &mesh) {
    const std::vector<uint32_t> unused = find_unused_vertices(mesh);
    remove_unused_vertices(mesh, unused);
    return unused.size();
}

template <glm::length_t n_dims, typename T>
size_t remove_triangles_of_negligible_size(
    mesh::Simple_<n_dims, T> &mesh,
    const std::type_identity_t<T> threshold_fraction_of_average) {
    const size_t triangle_count = mesh.triangles.size();

    std::vector<T> areas;
    areas.reserve(triangle_count);
    for (const glm::uvec3 &triangle : mesh.triangles) {
        const T area = geometry::compute_triangle_area(triangle, mesh.positions);
        areas.push_back(area);
    }

    const T total_area = std::reduce(areas.begin(), areas.end(), static_cast<T>(0));
    const T average_area = total_area / static_cast<T>(areas.size());

    // Zipping moves each area along with its triangle, so the predicate never has to recover an index.
    const T threshold = average_area * threshold_fraction_of_average;
    const auto removed = std::ranges::remove_if(
        std::views::zip(mesh.triangles, areas),
        [&](const T area) { return area < threshold; },
        [](const auto &triangle_and_area) { return std::get<1>(triangle_and_area); });
    const size_t erased_count = removed.size();
    mesh.triangles.resize(mesh.triangles.size() - erased_count);
    return erased_count;
}

}
