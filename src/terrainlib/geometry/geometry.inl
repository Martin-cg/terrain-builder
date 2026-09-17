#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/norm.hpp>
#include <libassert/assert.hpp>
#include <radix/geometry.h>

namespace geometry {

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_absolute(const radix::geometry::Aabb<n_dims, T> &bounds, const T padding) {
    using Vec = glm::vec<n_dims, T>;
    return radix::geometry::Aabb<n_dims, T>(bounds.min - Vec(padding), bounds.max + Vec(padding));
}

template <glm::length_t n_dims, typename T>
radix::geometry::Aabb<n_dims, T> pad_bounds_relative(const radix::geometry::Aabb<n_dims, T> &bounds, const T fraction, const T min_padding) {
    using Vec = glm::vec<n_dims, T>;
    const Vec padding = glm::max(bounds.size() * fraction, Vec(min_padding));
    return radix::geometry::Aabb<n_dims, T>(bounds.min - padding, bounds.max + padding);
}

template <glm::length_t n_dims>
void to_approximate_normalized(
    std::span<const glm::vec<n_dims, double>> positions,
    std::vector<glm::vec<n_dims, float>> &approx,
    radix::geometry::Aabb<n_dims, double> *out_bounds) {
    // compute bounds
    const radix::geometry::Aabb<n_dims, double> bounds = radix::geometry::find_bounds(positions);
    const glm::vec<n_dims, double> center = bounds.centre();
    const glm::vec<n_dims, double> extents = bounds.size() / 2.0;
    const double max_extents = glm::compMax(extents);
    // Guard against coincident positions.
    const double scale = max_extents > 0 ? 1.0 / max_extents : 1.0;

    if (out_bounds) {
        *out_bounds = bounds;
    }

    // normalize based on aabb
    approx.clear();
    approx.reserve(positions.size());
    for (const auto &p : positions) {
        const glm::vec<n_dims, double> rel = (p - center) * scale;
        approx.push_back(glm::vec<n_dims, float>(rel));
    }
}
template <glm::length_t n_dims>
void to_approximate_normalized(
    const std::vector<glm::vec<n_dims, double>> &positions,
    std::vector<glm::vec<n_dims, float>> &approx,
    radix::geometry::Aabb<n_dims, double> *out_bounds) {
    return to_approximate_normalized(std::span(positions), approx, out_bounds);
}

template <glm::length_t n_dims>
std::vector<glm::vec<n_dims, float>> to_approximate_normalized(
    std::span<const glm::vec<n_dims, double>> positions,
    radix::geometry::Aabb<n_dims, double> *out_bounds) {
    std::vector<glm::vec<n_dims, float>> approx;
    to_approximate_normalized(positions, approx, out_bounds);
    return approx;
}
template <glm::length_t n_dims>
std::vector<glm::vec<n_dims, float>> to_approximate_normalized(
    const std::vector<glm::vec<n_dims, double>> &positions,
    radix::geometry::Aabb<n_dims, double> *out_bounds) {
    return to_approximate_normalized(std::span(positions), out_bounds);
}

template <glm::length_t n_dims, typename T>
radix::geometry::Triangle<n_dims, T> corners(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions) {
    return {positions[triangle.x], positions[triangle.y], positions[triangle.z]};
}

template <glm::length_t n_dims, typename T>
radix::geometry::Triangle<n_dims, T> corners(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>> &positions) {
    return corners(triangle, std::span(positions));
}

template <typename T>
T orient(const glm::vec<2, T> &a, const glm::vec<2, T> &b, const glm::vec<2, T> &p) {
    return cross(b - a, p - a);
}

template <glm::length_t n_dims, typename T>
glm::vec<3, T> compute_barycentric(
    const glm::vec<n_dims, T> &point,
    const glm::vec<n_dims, T> &a,
    const glm::vec<n_dims, T> &b,
    const glm::vec<n_dims, T> &c) {
    if constexpr (n_dims == 2) {
        const T denom = orient(a, b, c);
        const T first = orient(b, c, point) / denom;
        const T second = orient(c, a, point) / denom;
        return {first, second, T(1) - first - second};
    } else {
        using Vec = glm::vec<n_dims, T>;
        using Vec2 = glm::vec<2, T>;
        using Mat2 = glm::mat<2, 2, T>;

        const Vec edge_ab = b - a;
        const Vec edge_ac = c - a;
        const Vec point_offset = point - a;

        const Mat2 gram{
            glm::dot(edge_ab, edge_ab),
            glm::dot(edge_ab, edge_ac),
            glm::dot(edge_ab, edge_ac),
            glm::dot(edge_ac, edge_ac)};
        DEBUG_ASSERT(glm::determinant(gram) != 0);

        const Vec2 projection{
            glm::dot(point_offset, edge_ab),
            glm::dot(point_offset, edge_ac)};

        const Vec2 weights_bc = glm::inverse(gram) * projection;
        const T weight_b = weights_bc.x;
        const T weight_c = weights_bc.y;
        const T weight_a = T(1) - weight_b - weight_c;

        return {weight_a, weight_b, weight_c};
    }
}

template <glm::length_t n_dims, typename T>
glm::vec<3, T> compute_barycentric(const glm::vec<n_dims, T> &point, const radix::geometry::Triangle<n_dims, T> &triangle) {
    return compute_barycentric(point, triangle[0], triangle[1], triangle[2]);
}

template <typename V, typename T>
V interpolate(const std::array<V, 3> &values, const glm::vec<3, T> &weights) {
    return weights[0] * values[0] + weights[1] * values[1] + weights[2] * values[2];
}

template <glm::length_t n_dims, typename T>
T compute_closest_along_segment(const glm::vec<n_dims, T> &point, const radix::geometry::Edge<n_dims, T> &segment) {
    constexpr T epsilon_squared = radix::geometry::epsilon<T> * radix::geometry::epsilon<T>;

    const glm::vec<n_dims, T> along = segment[1] - segment[0];
    const T length_squared = glm::dot(along, along);
    if (length_squared <= epsilon_squared) {
        return T(0);
    }
    return std::clamp(glm::dot(point - segment[0], along) / length_squared, T(0), T(1));
}

template <glm::length_t n_dims, typename T>
T distance_to_segment(const glm::vec<n_dims, T> &point, const radix::geometry::Edge<n_dims, T> &segment) {
    const T fraction = compute_closest_along_segment(point, segment);
    return glm::distance(point, segment[0] + fraction * (segment[1] - segment[0]));
}

template <typename T>
T distance_to_triangle(const glm::vec<2, T> &point, const radix::geometry::Triangle<2, T> &triangle) {
    const T orientation = cross(triangle[1] - triangle[0], triangle[2] - triangle[0]);

    bool inside = orientation != T(0);
    for (uint8_t corner = 0; inside && corner < 3; corner++) {
        const glm::vec<2, T> edge = triangle[(corner + 1) % 3] - triangle[corner];
        inside = cross(edge, point - triangle[corner]) * orientation >= T(0);
    }
    if (inside) {
        return T(0);
    }

    T nearest = std::numeric_limits<T>::max();
    for (uint8_t corner = 0; corner < 3; corner++) {
        const radix::geometry::Edge<2, T> edge = {triangle[corner], triangle[(corner + 1) % 3]};
        nearest = std::min(nearest, distance_to_segment(point, edge));
    }
    return nearest;
}

namespace detail {
// True when an edge normal of the first triangle separates the two. Touching does not separate.
template <typename T>
bool separates(const radix::geometry::Triangle<2, T> &triangle, const radix::geometry::Triangle<2, T> &other) {
    constexpr T epsilon_squared = radix::geometry::epsilon<T> * radix::geometry::epsilon<T>;

    const auto extent_along = [](const glm::vec<2, T> &axis, const radix::geometry::Triangle<2, T> &corners) {
        T lowest = std::numeric_limits<T>::max();
        T highest = std::numeric_limits<T>::lowest();
        for (const glm::vec<2, T> &corner : corners) {
            const T value = glm::dot(axis, corner);
            lowest = std::min(lowest, value);
            highest = std::max(highest, value);
        }
        return glm::vec<2, T>(lowest, highest);
    };

    for (uint8_t corner = 0; corner < 3; corner++) {
        const glm::vec<2, T> edge = triangle[(corner + 1) % 3] - triangle[corner];
        const glm::vec<2, T> axis(-edge.y, edge.x);
        if (glm::dot(axis, axis) <= epsilon_squared) {
            continue;
        }

        const glm::vec<2, T> own = extent_along(axis, triangle);
        const glm::vec<2, T> rest = extent_along(axis, other);
        if (own.y < rest.x || rest.y < own.x) {
            return true;
        }
    }
    return false;
}
} // namespace detail

template <typename T>
bool triangles_overlap(const radix::geometry::Triangle<2, T> &a, const radix::geometry::Triangle<2, T> &b) {
    return !detail::separates(a, b) && !detail::separates(b, a);
}

template <glm::length_t n_dims, typename T>
glm::vec<3, T> compute_closest_barycentric(const glm::vec<n_dims, T> &point, const std::array<glm::vec<n_dims, T>, 3> &triangle) {
    using Barycentric = glm::vec<3, T>;

    if (!is_empty_triangle(triangle)) {
        const Barycentric projected = compute_barycentric(point, triangle);
        if (glm::all(glm::greaterThanEqual(projected, Barycentric(0)))) {
            return projected;
        }
    }

    Barycentric nearest(0);
    T nearest_distance = std::numeric_limits<T>::max();
    for (uint8_t corner = 0; corner < 3; corner++) {
        const uint8_t next = (corner + 1) % 3;
        const T fraction = compute_closest_along_segment(
            point, radix::geometry::Edge<n_dims, T>{triangle[corner], triangle[next]});

        Barycentric barycentric(0);
        barycentric[corner] = T(1) - fraction;
        barycentric[next] = fraction;

        const T distance = glm::distance(point, interpolate(triangle, barycentric));
        if (distance < nearest_distance) {
            nearest_distance = distance;
            nearest = barycentric;
        }
    }
    return nearest;
}

template <typename T>
glm::vec<3, T> compute_normal(const glm::vec<3, T> &a,
                              const glm::vec<3, T> &b,
                              const glm::vec<3, T> &c,
                              const bool normalize) {
    const glm::vec<3, T> n = glm::cross(b - a, c - a);
    if (normalize && glm::length2(n) > T(0)) {
        return glm::normalize(n);
    }
    return n;
}

template <typename T>
glm::vec<3, T> compute_normal(const glm::uvec3 &triangle,
                              const std::span<const glm::vec<3, T>> positions,
                              const bool normalize) {
    const auto &a = positions[triangle[0]];
    const auto &b = positions[triangle[1]];
    const auto &c = positions[triangle[2]];
    return compute_normal(a, b, c, normalize);
}

template <typename T>
glm::vec<3, T> compute_triangle_angles(const radix::geometry::Triangle<3, T> &triangle) {
    std::array<glm::vec<3, T>, 3> edges;
    for (const uint8_t k : range<uint8_t>(3)) {
        const glm::vec<3, T> edge = triangle[(k + 1) % 3] - triangle[k];
        if (glm::length2(edge) == T(0)) {
            return glm::vec<3, T>(0);
        }
        edges[k] = glm::normalize(edge);
    }

    glm::vec<3, T> angles;
    for (const uint8_t k : range<uint8_t>(3)) {
        // The angle is enclosed by the edge leaving the vertex and the one arriving, which points
        // the other way, so the dot product of the two outgoing directions is negated.
        const T cosine = -glm::dot(edges[k], edges[(k + 2) % 3]);
        angles[k] = std::acos(std::clamp(cosine, T(-1), T(1)));
    }

    return angles;
}

template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2) {
    const glm::vec<n_dims, T> e1 = v1 - v0;
    const glm::vec<n_dims, T> e2 = v2 - v0;

    const T e1e1 = glm::dot(e1, e1);
    const T e2e2 = glm::dot(e2, e2);
    const T e1e2 = glm::dot(e1, e2);

    // squared parallelogram area = |e1|^2 |e2|^2 - (e1 . e2)^2
    // squared triangle area = squared parallelogram area / 4
    const T parallelogram_area2 = std::fma(e1e1, e2e2, -(e1e2 * e1e2));

    // Small negative values can occur from roundoff for nearly degenerate triangles.
    return std::max(T(0), parallelogram_area2) / T(4);
}
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle) {
    return compute_squared_triangle_area<n_dims, T>(triangle[0], triangle[1], triangle[2]);
}
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions) {
    const glm::vec<n_dims, T> &v0 = positions[triangle.x];
    const glm::vec<n_dims, T> &v1 = positions[triangle.y];
    const glm::vec<n_dims, T> &v2 = positions[triangle.z];
    return compute_squared_triangle_area<n_dims, T>(v0, v1, v2);
}
template <glm::length_t n_dims, typename T>
T compute_squared_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>>& positions) {
    return compute_squared_triangle_area<n_dims, T>(triangle, std::span(positions));
}

template <typename T>
T compute_longest_edge(const radix::geometry::Triangle<3, T> &triangle) {
    return std::sqrt(std::max({
        glm::distance2(triangle[0], triangle[1]),
        glm::distance2(triangle[1], triangle[2]),
        glm::distance2(triangle[2], triangle[0]),
    }));
}

template <typename T>
T compute_triangle_thickness(const radix::geometry::Triangle<3, T> &triangle) {
    const T longest_edge = compute_longest_edge(triangle);
    if (longest_edge == 0) {
        return 0;
    }
    // Twice the area is the altitude onto the longest edge times that edge, and that altitude is the smallest.
    return 2 * compute_triangle_area(triangle[0], triangle[1], triangle[2]) / longest_edge;
}

template <typename T>
T compute_signed_triangle_area(const glm::vec<2, T> &v0, const glm::vec<2, T> &v1, const glm::vec<2, T> &v2) {
    return orient(v0, v1, v2) / T(2);
}
template <typename T>
T compute_signed_triangle_area(const std::array<glm::vec<2, T>, 3> &triangle) {
    return compute_signed_triangle_area<T>(triangle[0], triangle[1], triangle[2]);
}
template <typename T>
T compute_signed_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<2, T>> positions) {
    return compute_signed_triangle_area<T>(positions[triangle.x], positions[triangle.y], positions[triangle.z]);
}
template <typename T>
T compute_signed_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<2, T>> &positions) {
    return compute_signed_triangle_area<T>(triangle, std::span<const glm::vec<2, T>>(positions));
}

template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2) {
    return std::sqrt(compute_squared_triangle_area(v0, v1, v2));
}
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const std::array<glm::vec<n_dims, T>, 3> &triangle) {
    return std::sqrt(compute_squared_triangle_area(triangle));
}
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions) {
    return std::sqrt(compute_squared_triangle_area(triangle, positions));
}
template <glm::length_t n_dims, typename T>
T compute_triangle_area(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>>& positions) {
    return std::sqrt(compute_squared_triangle_area(triangle, positions));
}

template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::vec<n_dims, T> &v0, const glm::vec<n_dims, T> &v1, const glm::vec<n_dims, T> &v2, const T min_area) {
    return compute_squared_triangle_area(v0, v1, v2) <= min_area * min_area;
}
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const std::array<glm::vec<n_dims, T>, 3> &triangle, const T min_area) {
    return compute_squared_triangle_area(triangle) <= min_area * min_area;
}
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::uvec3 &triangle, const std::span<const glm::vec<n_dims, T>> positions, const T min_area) {
    return compute_squared_triangle_area(triangle, positions) <= min_area * min_area;
}
template <glm::length_t n_dims, typename T>
bool is_empty_triangle(const glm::uvec3 &triangle, const std::vector<glm::vec<n_dims, T>> &positions, const T min_area) {
    return compute_squared_triangle_area(triangle, positions) <= min_area * min_area;
}

} // namespace geometry