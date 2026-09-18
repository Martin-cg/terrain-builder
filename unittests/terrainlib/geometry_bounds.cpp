#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "../catch2_helpers.h"

#include "geometry/bounds.h"

TEST_CASE("calculate_bounds takes each axis from its own extreme point", "[geometry][bounds]") {
    const std::vector<glm::dvec3> points = {
        {3.0, -1.0, 0.5},
        {-2.0, 5.0, 1.0},
        {0.0, 0.0, -4.0}};

    const radix::geometry::Aabb3d bounds = geometry::calculate_bounds(points);

    CHECK(bounds.min == glm::dvec3(-2.0, -1.0, -4.0));
    CHECK(bounds.max == glm::dvec3(3.0, 5.0, 1.0));
}

TEST_CASE("pad_bounds_absolute grows every side by the padding", "[geometry][bounds]") {
    const radix::geometry::Aabb3d bounds(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(4.0, 2.0, 1.0));

    const radix::geometry::Aabb3d padded = geometry::pad_bounds_absolute(bounds, 0.5);

    CHECK(padded.min == glm::dvec3(-0.5, -0.5, -0.5));
    CHECK(padded.max == glm::dvec3(4.5, 2.5, 1.5));
}

TEST_CASE("pad_bounds_relative grows each side by a fraction of the size along its axis", "[geometry][bounds]") {
    const radix::geometry::Aabb3d bounds(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(4.0, 2.0, 1.0));

    const radix::geometry::Aabb3d padded = geometry::pad_bounds_relative(bounds, 0.25);

    CHECK(padded.min == glm::dvec3(-1.0, -0.5, -0.25));
    CHECK(padded.max == glm::dvec3(5.0, 2.5, 1.25));
}

TEST_CASE("pad_bounds_relative grows each side by at least the minimum padding", "[geometry][bounds]") {
    const radix::geometry::Aabb3d bounds(glm::dvec3(0.0, 0.0, 0.0), glm::dvec3(4.0, 2.0, 1.0));

    const radix::geometry::Aabb3d padded = geometry::pad_bounds_relative(bounds, 0.25, 0.375);

    CHECK(padded.min == glm::dvec3(-1.0, -0.5, -0.375));
    CHECK(padded.max == glm::dvec3(5.0, 2.5, 1.375));
}
