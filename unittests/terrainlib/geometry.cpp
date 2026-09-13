#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "../catch2_helpers.h"

#include "geometry/geometry.h"

TEST_CASE("distance_to_segment clamps to the segment's ends", "[terrainlib][geometry]") {
    const radix::geometry::Edge<2, double> segment = {glm::dvec2(0.0, 0.0), glm::dvec2(4.0, 0.0)};

    CHECK(geometry::distance_to_segment(glm::dvec2(2.0, 3.0), segment) == Catch::Approx(3.0));
    CHECK(geometry::distance_to_segment(glm::dvec2(-3.0, 0.0), segment) == Catch::Approx(3.0));
    CHECK(geometry::distance_to_segment(glm::dvec2(9.0, 0.0), segment) == Catch::Approx(5.0));
    CHECK(geometry::distance_to_segment(glm::dvec2(1.0, 0.0), segment) == Catch::Approx(0.0));

    const radix::geometry::Edge<2, double> degenerate = {glm::dvec2(1.0, 1.0), glm::dvec2(1.0, 1.0)};
    CHECK(geometry::distance_to_segment(glm::dvec2(1.0, 4.0), degenerate) == Catch::Approx(3.0));
}

TEST_CASE("distance_to_triangle is zero inside and grows outside", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<2, double> triangle = {
        glm::dvec2(0.0, 0.0),
        glm::dvec2(4.0, 0.0),
        glm::dvec2(0.0, 4.0),
    };

    CHECK(geometry::distance_to_triangle(glm::dvec2(1.0, 1.0), triangle) == 0.0);
    CHECK(geometry::distance_to_triangle(glm::dvec2(0.0, 0.0), triangle) == 0.0);
    CHECK(geometry::distance_to_triangle(glm::dvec2(2.0, 0.0), triangle) == 0.0);
    CHECK(geometry::distance_to_triangle(glm::dvec2(0.0, -3.0), triangle) == Catch::Approx(3.0));
    CHECK(geometry::distance_to_triangle(glm::dvec2(-2.0, 2.0), triangle) == Catch::Approx(2.0));
}

TEST_CASE("distance_to_triangle accepts either winding", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<2, double> counter_clockwise = {
        glm::dvec2(0.0, 0.0),
        glm::dvec2(4.0, 0.0),
        glm::dvec2(0.0, 4.0),
    };
    const radix::geometry::Triangle<2, double> clockwise = {
        counter_clockwise[2],
        counter_clockwise[1],
        counter_clockwise[0],
    };

    CHECK(geometry::distance_to_triangle(glm::dvec2(1.0, 1.0), clockwise) == 0.0);
    CHECK(geometry::distance_to_triangle(glm::dvec2(0.0, -3.0), clockwise) == Catch::Approx(3.0));
}

TEST_CASE("triangles_overlap counts touching as overlapping", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<2, double> triangle = {
        glm::dvec2(0.0, 0.0),
        glm::dvec2(4.0, 0.0),
        glm::dvec2(0.0, 4.0),
    };

    CHECK(geometry::triangles_overlap(triangle, triangle));

    // Shifted so the two share only the corner at (4, 0).
    const radix::geometry::Triangle<2, double> touching = {
        glm::dvec2(4.0, 0.0),
        glm::dvec2(8.0, 0.0),
        glm::dvec2(4.0, -4.0),
    };
    CHECK(geometry::triangles_overlap(triangle, touching));

    const radix::geometry::Triangle<2, double> apart = {
        glm::dvec2(100.0, 100.0),
        glm::dvec2(104.0, 100.0),
        glm::dvec2(100.0, 104.0),
    };
    CHECK_FALSE(geometry::triangles_overlap(triangle, apart));

    // Contained entirely within the first, so no edge crosses any other.
    const radix::geometry::Triangle<2, double> inner = {
        glm::dvec2(0.5, 0.5),
        glm::dvec2(1.5, 0.5),
        glm::dvec2(0.5, 1.5),
    };
    CHECK(geometry::triangles_overlap(triangle, inner));
}
