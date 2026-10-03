#pragma once

#include <array>
#include <ranges>
#include <span>

#include "polygon/Polygon.h"
#include "mesh/SimpleMesh.h"

namespace polygon {

SimpleMesh3d triangulate(const Polygon3d &polygon);
void triangulate(SimpleMesh3d &mesh, const std::span<const uint32_t> indices);

// Triangulates the convex `polygon` as a fan around its first vertex.
// Passes each triangle to `output`.
template <std::ranges::random_access_range Polygon, typename OutputFn>
void triangulate_fan(const Polygon &polygon, OutputFn &&output) {
    for (size_t vertex = 2; vertex < std::ranges::size(polygon); vertex++) {
        const std::array<std::ranges::range_value_t<Polygon>, 3> triangle = {polygon[0], polygon[vertex - 1], polygon[vertex]};
        output(triangle);
    }
}

} // namespace polygon
