#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

namespace geometry {

using Triangle3d = radix::geometry::Triangle<3, double>;

// Grow bounds by the same distance on every side.
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_absolute(const radix::geometry::Aabb<n_dims, T> &bounds, const T padding);

// Grow bounds by the given fraction of their size on every side.
template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_relative(const radix::geometry::Aabb<n_dims, T> &bounds, const T fraction, const T min_padding = T(0));

// Normalize a set of positions into the range of [-1,1] based on maximum extents of the bounding box.
// Outputs are written as float coordinates.
// Optionally outputs the computed AABB if out_bounds is provided.
template <glm::length_t n_dims>
void to_approximate_normalized(
    std::span<const glm::vec<n_dims, double>> positions,
    std::vector<glm::vec<n_dims, float>> &approx,
    radix::geometry::Aabb<n_dims, double> *out_bounds = nullptr);
template <glm::length_t n_dims>
void to_approximate_normalized(
    const std::vector<glm::vec<n_dims, double>> &positions,
    std::vector<glm::vec<n_dims, float>> &approx,
    radix::geometry::Aabb<n_dims, double> *out_bounds = nullptr);

// Normalize a set of positions into the range of [-1,1] based on maximum extents of the bounding box.
// Outputs are written as float coordinates.
// Optionally outputs the computed AABB if out_bounds is provided.
template <glm::length_t n_dims>
std::vector<glm::vec<n_dims, float>> to_approximate_normalized(
    std::span<const glm::vec<n_dims, double>> positions,
    radix::geometry::Aabb<n_dims, double> *out_bounds = nullptr);
template <glm::length_t n_dims>
std::vector<glm::vec<n_dims, float>> to_approximate_normalized(
    const std::vector<glm::vec<n_dims, double>> &positions,
    radix::geometry::Aabb<n_dims, double> *out_bounds = nullptr);

template <glm::length_t n_dims, typename T>
radix::geometry::Triangle<n_dims, T> corners(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions);

// Scalar z-component in 2D, full cross product vector in 3D.
template <glm::length_t n_dims, typename T>
auto cross(const glm::vec<n_dims, T> &a, const glm::vec<n_dims, T> &b) {
    if constexpr (n_dims == 2) {
        return a.x * b.y - a.y * b.x;
    } else if constexpr (n_dims == 3) {
        return glm::cross(a, b);
    } else {
        static_assert(n_dims == 2 || n_dims == 3, "cross is only defined for 2D and 3D vectors");
    }
}

// Positive when p lies left of the directed edge a to b, and zero when the three are collinear.
template <typename T>
T orient(const glm::vec<2, T> &a, const glm::vec<2, T> &b, const glm::vec<2, T> &p);

// Corner weights of the point, summing to one. Meaningless for a degenerate triangle.
template <glm::length_t n_dims, typename T>
glm::vec<3, T> compute_barycentric(
    const glm::vec<n_dims, T> &point,
    const glm::vec<n_dims, T> &a,
    const glm::vec<n_dims, T> &b,
    const glm::vec<n_dims, T> &c);
template <glm::length_t n_dims, typename T>
glm::vec<3, T> compute_barycentric(const glm::vec<n_dims, T> &point, const radix::geometry::Triangle<n_dims, T> &triangle);

template <typename V, typename T>
V interpolate(const std::array<V, 3> &values, const glm::vec<3, T> &weights);

template <glm::length_t n_dims, typename T>
T distance_to_segment(const glm::vec<n_dims, T> &point, const radix::geometry::Edge<n_dims, T> &segment);

// Zero inside the triangle, growing outside it. Accepts either winding.
template <typename T>
T distance_to_triangle(const glm::vec<2, T> &point, const radix::geometry::Triangle<2, T> &triangle);

// Separating axis test over the six edge normals. Triangles that only touch count as overlapping.
template <typename T>
bool triangles_overlap(const radix::geometry::Triangle<2, T> &a, const radix::geometry::Triangle<2, T> &b);

template <typename T>
constexpr T epsilon() {
    if constexpr (std::is_same_v<T, float>) {
        return 1e-6f;
    } else if constexpr (std::is_same_v<T, double>) {
        return 1e-12;
    } else {
        static_assert(sizeof(T) == 0, "Unsupported type for epsilon");
    }
}
template <typename T>
constexpr T epsilon_sq() {
    return epsilon<T>() * epsilon<T>();
}

template <typename T>
glm::vec<3, T> compute_normal(const glm::vec<3, T> &a,
                              const glm::vec<3, T> &b,
                              const glm::vec<3, T> &c,
                              const bool normalize = true);
template <typename T>
glm::vec<3, T> compute_normal(const glm::uvec3 &triangle,
                              const std::span<const glm::vec<3, T>> positions,
                              const bool normalize = true);

template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2);
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle);
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions);
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>>& positions);

// The longest of the triangle's three edges.
template <typename T>
T compute_longest_edge(const radix::geometry::Triangle<3, T> &triangle);

// The triangle's smallest altitude, zero where its corners coincide.
template <typename T>
T compute_triangle_thickness(const radix::geometry::Triangle<3, T> &triangle);

// Negative where the triangle winds the other way.
template <typename T>
T compute_signed_triangle_area(const glm::vec<2, T> &v0, const glm::vec<2, T> &v1, const glm::vec<2, T> &v2);
template <typename T>
T compute_signed_triangle_area(const std::array<glm::vec<2, T>, 3> &triangle);
template <typename T>
T compute_signed_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<2, T>> positions);
template <typename T>
T compute_signed_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<2, T>> &positions);

template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2);
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle);
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions);
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>> &positions);

template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2, const T min_area = 2 * epsilon<T>());
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const std::array<glm::vec<n_dims, T>, 3> &triangle, const T min_area = 2 * epsilon<T>());
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions, const T min_area = 2 * epsilon<T>());
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>> &positions, const T min_area = 2 * epsilon<T>());

} // namespace geometry

#include "geometry.inl"