#pragma once

#include <algorithm>
#include <functional>
#include <ranges>
#include <type_traits>
#include <vector>

#include <libassert/assert.hpp>

#include "mesh/SimpleMesh.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
void combine_inplace(SimpleMesh_<n_dims, T> &acc_mesh, const SimpleMesh_<n_dims, T> &other_mesh) {
    DEBUG_ASSERT(acc_mesh.vertex_count() == 0 || other_mesh.vertex_count() == 0 || acc_mesh.has_uvs() == other_mesh.has_uvs());

    const uint32_t vertex_offset = static_cast<uint32_t>(acc_mesh.vertex_count());
    acc_mesh.positions.append_range(other_mesh.positions);
    acc_mesh.uvs.append_range(other_mesh.uvs);
    acc_mesh.triangles.append_range(other_mesh.triangles | std::views::transform([&](const glm::uvec3 &t) {
        return t + vertex_offset;
    }));
}

template <std::ranges::forward_range Range>
auto combine(const Range &meshes, std::vector<size_t> &vertex_offsets) {
    using Mesh = std::remove_cvref_t<std::unwrap_reference_t<std::ranges::range_value_t<Range>>>;

    size_t combined_vertex_count = 0;
    size_t combined_triangle_count = 0;
    for (const Mesh &mesh : meshes) {
        combined_vertex_count += mesh.vertex_count();
        combined_triangle_count += mesh.face_count();
    }

    const bool has_uvs = std::ranges::any_of(meshes, &Mesh::has_uvs);

    Mesh result;
    result.positions.reserve(combined_vertex_count);
    result.triangles.reserve(combined_triangle_count);
    if (has_uvs) {
        result.uvs.reserve(combined_vertex_count);
    }

    vertex_offsets.clear();
    for (const Mesh &mesh : meshes) {
        vertex_offsets.push_back(result.vertex_count());
        combine_inplace(result, mesh);
    }

    return result;
}

template <std::ranges::forward_range Range>
auto combine(const Range &meshes) {
    std::vector<size_t> vertex_offsets;
    return combine(meshes, vertex_offsets);
}
}
