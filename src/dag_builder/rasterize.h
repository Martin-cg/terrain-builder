#pragma once

#include <array>
#include <concepts>
#include <cstdint>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "fixed_point.h"
#include "geometry/geometry.h"
#include "mesh/WindingOrder.h"
#include "range_utils.h"

namespace raster {

using FixedCoord = fixed::Q<16>;
using FixedPoint = fp::Vec2<FixedCoord>;
using FixedScalar = fp::Scalar<FixedCoord>;
using FixedTriangle = std::array<FixedPoint, 3>;

// The triangle with each corner rounded onto the fixed-point grid.
inline FixedTriangle make_triangle(const glm::dvec2 a, const glm::dvec2 b, const glm::dvec2 c) {
    return {FixedPoint::from_real(a), FixedPoint::from_real(b), FixedPoint::from_real(c)};
}

// The triangle with each corner rounded onto the fixed-point grid.
inline FixedTriangle make_triangle(const geometry::Triangle2d &triangle) {
    return make_triangle(triangle[0], triangle[1], triangle[2]);
}

// The centre of a grid cell. Exactly representable, so no rounding happens.
inline FixedPoint get_cell_center(const glm::uvec2 cell) {
    const FixedCoord::repr_t half = FixedCoord::scale / 2;
    return FixedPoint{{
        FixedCoord::from_integer(FixedCoord::repr_t{cell.x}).value + half,
        FixedCoord::from_integer(FixedCoord::repr_t{cell.y}).value + half}};
}

namespace detail {

inline radix::geometry::Aabb2ui clamp_bounds(
    const radix::geometry::Aabb2ui &bounds,
    const radix::geometry::Aabb2ui &clamp_to) {
    return radix::geometry::Aabb2ui(
        glm::clamp(bounds.min, clamp_to.min, clamp_to.max),
        glm::clamp(bounds.max, clamp_to.min, clamp_to.max));
}

// The cells the triangle can reach, clipped to the grid.
inline radix::geometry::Aabb2ui get_cell_bounds(
    const FixedTriangle &triangle,
    const glm::uvec2 grid_size) {
    const FixedPoint lo = fp::min(triangle[0], triangle[1], triangle[2]);
    const FixedPoint hi = fp::max(triangle[0], triangle[1], triangle[2]);

    // A negative coordinate wraps when converted, so it is cut before the conversion.
    const radix::geometry::Aabb2ui cell_bounds(
        glm::uvec2(glm::max(lo.real(), glm::dvec2(0.0))),
        glm::uvec2(glm::max(hi.real(), glm::dvec2(0.0))));

    const radix::geometry::Aabb2ui grid_bounds(
        glm::uvec2(0, 0),
        grid_size - glm::uvec2(1, 1));

    return clamp_bounds(cell_bounds, grid_bounds);
}

// Edge function of a directed edge, evaluated incrementally over the cell grid.
// The value is the raw cross product (edge_end - edge_start) x (center - edge_start)
// without rescaling, so its sign is the exact orientation of the cell centre.
struct EdgeFunction {
    FixedCoord::repr_t value = 0;
    FixedCoord::repr_t step_x = 0;
    FixedCoord::repr_t step_y = 0;

    EdgeFunction(const FixedPoint edge_start, const FixedPoint edge_end, const FixedPoint center) {
        const auto edge = edge_end.raw - edge_start.raw;
        const auto offset = center.raw - edge_start.raw;
        this->value = edge.x * offset.y - edge.y * offset.x;
        this->step_x = -edge.y * FixedCoord::scale;
        this->step_y = edge.x * FixedCoord::scale;
    }
};

} // namespace detail

// Visits every cell of the grid whose centre lies in the triangle, the cells of a row in turn.
// Either winding is accepted; a degenerate triangle covers nothing.
template <std::invocable<glm::uvec2> Visitor>
void scan_triangle(const FixedTriangle &triangle, const glm::uvec2 grid_size, Visitor &&visit) {
    const FixedScalar area2 = fp::orient(triangle[0], triangle[1], triangle[2]);
    if (area2 == FixedScalar::zero()) {
        return;
    }
    const FixedTriangle wound = area2 < FixedScalar::zero() ? swap_orientation(triangle) : triangle;

    const radix::geometry::Aabb2ui bounds = detail::get_cell_bounds(wound, grid_size);
    const FixedPoint start = get_cell_center({bounds.min.x, bounds.min.y});

    std::array<detail::EdgeFunction, 3> edges = {
        detail::EdgeFunction(wound[0], wound[1], start),
        detail::EdgeFunction(wound[1], wound[2], start),
        detail::EdgeFunction(wound[2], wound[0], start)};

    for (uint32_t y = bounds.min.y; y <= bounds.max.y; y++) {
        std::array<FixedCoord::repr_t, 3> row = {edges[0].value, edges[1].value, edges[2].value};

        for (uint32_t x = bounds.min.x; x <= bounds.max.x; x++) {
            if (row[0] >= 0 && row[1] >= 0 && row[2] >= 0) {
                visit(glm::uvec2(x, y));
            }

            for (const uint8_t k : range<uint8_t>(3)) {
                row[k] += edges[k].step_x;
            }
        }

        for (const uint8_t k : range<uint8_t>(3)) {
            edges[k].value += edges[k].step_y;
        }
    }
}

} // namespace raster
