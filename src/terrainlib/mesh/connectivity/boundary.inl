#include <span>
#include <unordered_set>

#include <glm/glm.hpp>

#include "mesh/SimpleMesh.h"
#include "mesh/connectivity/edges.h"
#include "mesh/connectivity/vertex_index_range.h"
#include "mesh/normalize.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> find_boundary_edges(const mesh::View_<n_dims, T> &mesh) {
    return find_boundary_edges(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_set<glm::uvec2> find_boundary_edges(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundary_edges(mesh.triangles);
}
inline std::unordered_set<glm::uvec2> find_boundary_edges(const std::span<const glm::uvec3> triangles) {
    std::unordered_set<glm::uvec2> boundary;
    find_boundary_edges(triangles, boundary);
    return boundary;
}

template <glm::length_t n_dims, typename T>
std::unordered_set<uint32_t> find_boundary_triangles(const mesh::View_<n_dims, T> &mesh) {
    return find_boundary_triangles(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_set<uint32_t> find_boundary_triangles(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundary_triangles(mesh.triangles);
}
inline std::unordered_set<uint32_t> find_boundary_triangles(const std::span<const glm::uvec3> triangles) {
    std::unordered_set<uint32_t> boundary;
    find_boundary_triangles(triangles, boundary);
    return boundary;
}

template <glm::length_t n_dims, typename T>
std::unordered_set<uint32_t> find_boundary_vertices(const mesh::View_<n_dims, T> &mesh) {
    return find_boundary_vertices(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::unordered_set<uint32_t> find_boundary_vertices(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundary_vertices(mesh.triangles);
}
inline std::unordered_set<uint32_t> find_boundary_vertices(const std::span<const glm::uvec3> triangles) {
    std::unordered_set<uint32_t> boundary;
    find_boundary_vertices(triangles, boundary);
    return boundary;
}

template <glm::length_t n_dims, typename T>
inline std::vector<uint8_t> build_boundary_vertex_mask(const mesh::View_<n_dims, T> &mesh) {
    return build_boundary_vertex_mask(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
inline std::vector<uint8_t> build_boundary_vertex_mask(const mesh::Simple_<n_dims, T> &mesh) {
    return build_boundary_vertex_mask(mesh.triangles, mesh.vertex_count());
}
inline std::vector<uint8_t> build_boundary_vertex_mask(const std::span<const glm::uvec3> triangles, size_t vertex_count) {
    std::vector<uint8_t> mask;
    build_boundary_vertex_mask<uint8_t>(triangles, vertex_count, mask);
    return mask;
}

template <typename MaskT>
inline void build_boundary_vertex_mask(const std::span<const glm::uvec3> triangles,
                                      const size_t vertex_count,
                                      std::vector<MaskT> &boundary,
                                      const MaskT on_boundary,
                                      const MaskT not_on_boundary) {
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));
    std::unordered_set<glm::uvec2> boundary_edges = find_boundary_edges(triangles);

    boundary.assign(vertex_count, not_on_boundary);
    for (const glm::uvec2 edge : boundary_edges) {
        boundary[edge.x] = on_boundary;
        boundary[edge.y] = on_boundary;
    }
}

namespace detail {
template <typename F>
void for_each_boundary_halfedge(const std::span<const glm::uvec3> triangles, F &&func) {
    std::unordered_map<glm::uvec2, uint32_t> triangle_counts;
    for_each_halfedge(triangles, [&](const glm::uvec2 &edge) {
        triangle_counts[edge]++;
    }, /* normalize */ true);

    for_each_halfedge(triangles, [&](const glm::uvec2 &edge, const uint32_t triangle_index) {
        if (triangle_counts.at(normalize_edge(edge)) == 1) {
            func(edge, triangle_index);
        }
    }, /* normalize */ false);
}

inline std::unordered_map<glm::uvec2, uint32_t> find_boundary_edges_and_triangles(const std::span<const glm::uvec3> triangles) {
    std::unordered_map<glm::uvec2, uint32_t> edge_to_triangle;
    for_each_boundary_halfedge(triangles, [&](const glm::uvec2 &edge, const uint32_t triangle_index) {
        edge_to_triangle.emplace(edge, triangle_index);
    });
    return edge_to_triangle;
}
}

template <glm::length_t n_dims, typename T>
inline std::vector<uint8_t> build_boundary_triangle_mask(const mesh::View_<n_dims, T> &mesh) {
    return build_boundary_triangle_mask(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
inline std::vector<uint8_t> build_boundary_triangle_mask(const mesh::Simple_<n_dims, T> &mesh) {
    return build_boundary_triangle_mask(mesh.triangles);
}
inline std::vector<uint8_t> build_boundary_triangle_mask(const std::span<const glm::uvec3> triangles) {
    std::vector<uint8_t> mask;
    build_boundary_triangle_mask<uint8_t>(triangles, mask);
    return mask;
}

template <typename MaskT>
inline void build_boundary_triangle_mask(const std::span<const glm::uvec3> triangles,
                                        std::vector<MaskT> &boundary,
                                        const MaskT on_boundary,
                                        const MaskT not_on_boundary) {
    boundary.assign(triangles.size(), not_on_boundary);
    detail::for_each_boundary_halfedge(triangles, [&](const glm::uvec2 &, const uint32_t triangle_index) {
        boundary[triangle_index] = on_boundary;
    });
}

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_boundaries(const mesh::View_<n_dims, T> &mesh) {
    return find_boundaries(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_boundaries(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundaries(mesh.triangles);
}

template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_boundaries_non_manifold(const mesh::View_<n_dims, T> &mesh) {
    return find_boundaries_non_manifold(mesh.triangles);
}
template <glm::length_t n_dims, typename T>
std::vector<std::vector<uint32_t>> find_boundaries_non_manifold(const mesh::Simple_<n_dims, T> &mesh) {
    return find_boundaries_non_manifold(mesh.triangles);
}

}
