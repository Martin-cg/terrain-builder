#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "containers/UnionFind.h"
#include "enumerate.h"
#include "mesh/SimpleMesh.h"
#include "mesh/split.h"
#include "mesh/reindex.h"
#include "mesh/connectivity/vertex_index_range.h"

namespace mesh {
namespace detail {
using VertexIndex = uint32_t;
using ComponentIndex = uint32_t;
}

struct ComponentsIndex {
    std::vector<detail::ComponentIndex> vertex_to_component;
    size_t component_count;

    bool operator==(const ComponentsIndex &) const = default;
};

namespace detail {
inline UnionFind build_union_find(const std::span<const glm::uvec3> &triangles, const size_t vertex_count) {
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));

    UnionFind components(vertex_count);
    for (const glm::uvec3 &triangle : triangles) {
        components.make_union(triangle[0], triangle[1]);
        components.make_union(triangle[1], triangle[2]);
    }
    return components;
}
}

// Finds the connected components of `triangles`, where triangles sharing a vertex are connected.
inline ComponentsIndex find_connected_components(const std::span<const glm::uvec3> &triangles, const size_t vertex_count) {
    if (vertex_count == 0) {
        return {};
    }

    // Union vertices that belong to the same triangle
    UnionFind components = detail::build_union_find(triangles, vertex_count);

    // Convert to vertex to component index mapping
    detail::ComponentIndex next_index = 0;

    const detail::ComponentIndex invalid_component = std::numeric_limits<detail::ComponentIndex>::max();
    std::vector<detail::ComponentIndex> vertex_to_component(vertex_count, invalid_component);

    for (detail::VertexIndex vertex_index = 0; vertex_index < vertex_count; vertex_index++) {
        const detail::VertexIndex rep = components.find(vertex_index);
        uint32_t& component_index = vertex_to_component[rep];
        if (component_index == invalid_component) {
            // representative has not been encountered yet
            component_index = next_index;
            next_index++;
        }
        // vertex is in the same component as its represetative
        vertex_to_component[vertex_index] = component_index;
    }

    return ComponentsIndex{
        .vertex_to_component = vertex_to_component,
        .component_count = next_index};
}
template <glm::length_t n_dims, typename T>
ComponentsIndex find_connected_components(const mesh::View_<n_dims, T> &mesh) {
    return find_connected_components(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
ComponentsIndex find_connected_components(const mesh::Simple_<n_dims, T> &mesh) {
    return find_connected_components(mesh.triangles, mesh.vertex_count());
}

template <glm::length_t n_dims, typename T>
struct ComponentsAndMap {
    std::vector<mesh::Simple_<n_dims, T>> components;
    std::vector<uint32_t> vertex_remap;
};

template <glm::length_t n_dims, typename T>
ComponentsAndMap<n_dims, T> split_into_connected_components_with_map(const mesh::View_<n_dims, T> &mesh, const ComponentsIndex& components_index) {
    const auto &[vertex_to_component, component_count] = components_index;
    auto result = split_by_vertex(mesh, component_count, vertex_to_component, false);
    return ComponentsAndMap<n_dims, T>{
        .components = std::move(result.groups),
        .vertex_remap = std::move(result.vertex_remap)};
}
template <glm::length_t n_dims, typename T>
ComponentsAndMap<n_dims, T> split_into_connected_components_with_map(const mesh::Simple_<n_dims, T> &mesh, const ComponentsIndex &components_index) {
    return split_into_connected_components_with_map(mesh::View_<n_dims, T>(mesh), components_index);
}
template <glm::length_t n_dims, typename T>
std::vector<mesh::Simple_<n_dims, T>> split_into_connected_components(const mesh::View_<n_dims, T> &mesh, const ComponentsIndex &components_index) {
    return split_into_connected_components_with_map(mesh, components_index).components;
}
template <glm::length_t n_dims, typename T>
std::vector<mesh::Simple_<n_dims, T>> split_into_connected_components(const mesh::View_<n_dims, T> &mesh) {
    const ComponentsIndex& components_index = find_connected_components(mesh);
    return split_into_connected_components(mesh, components_index);
}
template <glm::length_t n_dims, typename T>
std::vector<mesh::Simple_<n_dims, T>> split_into_connected_components(const mesh::Simple_<n_dims, T> &mesh) {
    return split_into_connected_components(mesh::View_<n_dims, T>(mesh));
}
inline std::vector<std::vector<glm::uvec3>> split_into_connected_components(const std::span<const glm::uvec3> triangles) {
    const uint32_t vertex_count = vertex_buffer_size(triangles);
    const auto [vertex_to_component, component_count] = find_connected_components(triangles, vertex_count);
    std::vector<std::vector<glm::uvec3>> triangles_per_component;
    triangles_per_component.resize(component_count);
    for (const glm::uvec3 &triangle : triangles) {
        const uint32_t component_index = vertex_to_component[triangle[0]];
        DEBUG_ASSERT(component_index == vertex_to_component[triangle[1]]);
        DEBUG_ASSERT(component_index == vertex_to_component[triangle[2]]);
        std::vector<glm::uvec3> &component = triangles_per_component[component_index];
        component.push_back(triangle);
    }
    return triangles_per_component;
}

inline size_t count_connected_components(const std::span<const glm::uvec3> triangles, const size_t vertex_count) {
    const UnionFind components = detail::build_union_find(triangles, vertex_count);
    return components.set_count();
}
template <glm::length_t n_dims, typename T>
size_t count_connected_components(const mesh::View_<n_dims, T> &mesh) {
    return count_connected_components(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
size_t count_connected_components(const mesh::Simple_<n_dims, T> &mesh) {
    return count_connected_components(mesh.triangles, mesh.vertex_count());
}

inline bool is_single_component(const std::span<const glm::uvec3> &triangles, const size_t vertex_count) {
    DEBUG_ASSERT(vertex_count >= vertex_buffer_size(triangles));
    return count_connected_components(triangles, vertex_count) <= 1;
}
inline bool is_single_component(const std::span<const glm::uvec3> &triangles) {
    return is_single_component(triangles, vertex_buffer_size(triangles));
}
template <glm::length_t n_dims, typename T>
bool is_single_component(const mesh::View_<n_dims, T> &mesh) {
    return is_single_component(mesh.triangles, mesh.vertex_count());
}
template <glm::length_t n_dims, typename T>
bool is_single_component(const mesh::Simple_<n_dims, T> &mesh) {
    return is_single_component(mesh.triangles, mesh.vertex_count());
}

} // namespace mesh
