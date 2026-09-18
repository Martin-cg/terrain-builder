#include <functional>
#include <ranges>
#include <span>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "geometry/bounds.h"
#include "mesh/SimpleMesh.h"

namespace mesh {

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const SimpleMesh_<n_dims, T> &mesh) {
    return geometry::calculate_bounds(mesh.positions);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const SimpleMesh_<n_dims, T>> meshes) {
    const auto positions = meshes
        | std::views::transform([](const SimpleMesh_<n_dims, T> &mesh) -> const auto & { return mesh.positions; })
        | std::views::join;
    return geometry::calculate_bounds(positions);
}
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> calculate_bounds(const std::span<const std::reference_wrapper<const SimpleMesh_<n_dims, T>>> meshes) {
    const auto positions = meshes
        | std::views::transform([](const SimpleMesh_<n_dims, T> &mesh) -> const auto & { return mesh.positions; })
        | std::views::join;
    return geometry::calculate_bounds(positions);
}

}
