#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

#include <glm/glm.hpp>

#include "containers/SegmentedBuffer.h"
#include "mesh/SimpleMesh.h"
#include "mesh/View.h"

namespace mesh {

// Groups of triangles with the same vertices, each with at least two members in ascending index order, ordered by their second member.
// Without ignore_orientation, triangles match only when one is a rotation of the other.
SegmentedBuffer<uint32_t, uint32_t> find_duplicate_triangle_groups(
    std::span<const glm::uvec3> triangles,
    const bool ignore_orientation);

template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::Simple_<3, T> &mesh, const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(const mesh::View_<3, T> &mesh, const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions,
    const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles(
    const std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    const bool ignore_orientation);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(const mesh::Simple_<3, T> &mesh);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(const mesh::View_<3, T> &mesh);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_ignore_orientation(
    const std::span<const glm::uvec3> triangles,
    const std::span<const glm::vec<3, T>> positions);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const mesh::Simple_<3, T> &mesh);
template <typename T>
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const mesh::View_<3, T> &mesh);
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::vector<glm::uvec3> &triangles);
std::vector<uint32_t> find_duplicate_triangles_consider_orientation(const std::span<const glm::uvec3> triangles);

template <typename T>
void remove_duplicate_triangles(mesh::Simple_<3, T> &mesh, const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions,
    const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions,
    const bool ignore_orientation);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::span<const glm::vec<3, T>> positions);
template <typename T>
void remove_duplicate_triangles_ignore_orientation(
    std::vector<glm::uvec3> &triangles,
    const std::vector<glm::vec<3, T>> &positions);
template <typename T>
void remove_duplicate_triangles_consider_orientation(mesh::Simple_<3, T> &mesh);
void remove_duplicate_triangles_consider_orientation(std::vector<glm::uvec3> &triangles);

template <glm::length_t n_dims, typename T>
size_t remove_isolated_vertices(SimpleMesh_<n_dims, T> &mesh);

template <glm::length_t n_dims, typename T>
size_t remove_triangles_of_negligible_size(
    SimpleMesh_<n_dims, T> &mesh,
    const std::type_identity_t<T> threshold_fraction_of_average);

void remove_degenerate_triangles(std::vector<glm::uvec3> &triangles);

}

#include "cleanup.inl"
