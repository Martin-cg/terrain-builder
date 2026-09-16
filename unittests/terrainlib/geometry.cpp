#include <cmath>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "../catch2_helpers.h"

#include "geometry/geometry.h"

namespace {

void check_closest(const radix::geometry::Triangle<3, double> &triangle, const glm::dvec3 &point, const glm::dvec3 &expected) {
    const glm::dvec3 weights = geometry::compute_closest_barycentric(point, triangle);
    CHECK_THAT(glm::distance(weights, expected), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
}

} // namespace

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

TEST_CASE("compute_closest_along_segment clamps the fraction to the segment", "[terrainlib][geometry]") {
    const radix::geometry::Edge<2, double> segment = {glm::dvec2(0.0, 0.0), glm::dvec2(4.0, 0.0)};

    CHECK(geometry::compute_closest_along_segment(glm::dvec2(1.0, 3.0), segment) == 0.25);
    CHECK(geometry::compute_closest_along_segment(glm::dvec2(-3.0, 1.0), segment) == 0.0);
    CHECK(geometry::compute_closest_along_segment(glm::dvec2(9.0, -1.0), segment) == 1.0);
}

TEST_CASE("compute_closest_along_segment takes the start of an empty segment", "[terrainlib][geometry]") {
    const radix::geometry::Edge<2, double> degenerate = {glm::dvec2(1.0, 1.0), glm::dvec2(1.0, 1.0)};

    CHECK(geometry::compute_closest_along_segment(glm::dvec2(1.0, 4.0), degenerate) == 0.0);
}

TEST_CASE("compute_closest_barycentric projects a point above the face", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<3, double> triangle = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0),
    };
    const glm::dvec3 weights = geometry::compute_closest_barycentric(glm::dvec3(0.25, 0.25, 5.0), triangle);

    CHECK_THAT(glm::distance(weights, glm::dvec3(0.5, 0.25, 0.25)), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
}

TEST_CASE("compute_closest_barycentric clamps to the nearest corner", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<3, double> triangle = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0),
    };

    check_closest(triangle, glm::dvec3(-1.0, -1.0, 0.0), glm::dvec3(1.0, 0.0, 0.0));
    check_closest(triangle, glm::dvec3(2.0, -1.0, 0.0), glm::dvec3(0.0, 1.0, 0.0));
    check_closest(triangle, glm::dvec3(-1.0, 2.0, 0.0), glm::dvec3(0.0, 0.0, 1.0));
}

TEST_CASE("compute_closest_barycentric clamps to the nearest edge", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<3, double> triangle = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0),
    };

    check_closest(triangle, glm::dvec3(0.5, -1.0, 0.0), glm::dvec3(0.5, 0.5, 0.0));
    check_closest(triangle, glm::dvec3(-1.0, 0.5, 0.0), glm::dvec3(0.5, 0.0, 0.5));
    check_closest(triangle, glm::dvec3(1.0, 1.0, 0.0), glm::dvec3(0.0, 0.5, 0.5));
}

TEST_CASE("compute_closest_barycentric names the point it weights", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<3, double> triangle = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0),
    };
    const glm::dvec3 weights = geometry::compute_closest_barycentric(glm::dvec3(0.75, 0.75, 3.0), triangle);

    CHECK_THAT(glm::distance(geometry::interpolate(triangle, weights), glm::dvec3(0.5, 0.5, 0.0)), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
}

TEST_CASE("compute_closest_barycentric falls to the edges of an empty triangle", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<3, double> collinear = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 0.0, 0.0),
        glm::dvec3(2.0, 0.0, 0.0),
    };
    const glm::dvec3 weights = geometry::compute_closest_barycentric(glm::dvec3(0.5, 3.0, 0.0), collinear);

    CHECK_THAT(glm::distance(geometry::interpolate(collinear, weights), glm::dvec3(0.5, 0.0, 0.0)), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
}

TEST_CASE("compute_closest_barycentric works in two dimensions", "[terrainlib][geometry]") {
    const radix::geometry::Triangle<2, double> triangle = {
        glm::dvec2(0.0, 0.0),
        glm::dvec2(4.0, 0.0),
        glm::dvec2(0.0, 4.0),
    };

    // The nearest point to (-2, 2) is (0, 2), halfway along the edge between the first and last corner.
    const glm::dvec3 weights = geometry::compute_closest_barycentric(glm::dvec2(-2.0, 2.0), triangle);

    CHECK_THAT(glm::distance(weights, glm::dvec3(0.5, 0.0, 0.5)), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
}

TEST_CASE("compute_longest_edge picks the longest of the three edges", "[terrainlib][geometry]") {
    const geometry::Triangle3d triangle = {glm::dvec3(0, 0, 0), glm::dvec3(3, 0, 0), glm::dvec3(0, 4, 0)};

    CHECK(geometry::compute_longest_edge(triangle) == 5.0);
}

TEST_CASE("compute_triangle_thickness is the smallest altitude", "[terrainlib][geometry]") {
    SECTION("equilateral") {
        const geometry::Triangle3d triangle = {glm::dvec3(0, 0, 0), glm::dvec3(2, 0, 0), glm::dvec3(1, std::sqrt(3.0), 0)};
        CHECK_THAT(geometry::compute_triangle_thickness(triangle), Catch::Matchers::WithinRel(std::sqrt(3.0)));
    }
    SECTION("right angled") {
        // The altitudes are 4, 3 and 2.4, the last onto the hypotenuse.
        const geometry::Triangle3d triangle = {glm::dvec3(0, 0, 0), glm::dvec3(3, 0, 0), glm::dvec3(0, 4, 0)};
        CHECK_THAT(geometry::compute_triangle_thickness(triangle), Catch::Matchers::WithinRel(2.4));
    }
    SECTION("cap a hair off collinear") {
        const geometry::Triangle3d triangle = {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0.5, 1e-5, 0)};
        CHECK_THAT(geometry::compute_triangle_thickness(triangle), Catch::Matchers::WithinAbs(1e-5, geometry::epsilon<double>()));
    }
}

TEST_CASE("compute_triangle_thickness is zero for degenerate triangles", "[terrainlib][geometry]") {
    const auto [name, triangle] = GENERATE(
        std::pair{"collinear corners", geometry::Triangle3d{glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(3, 0, 0)}},
        std::pair{"repeated corner", geometry::Triangle3d{glm::dvec3(0, 0, 0), glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0)}},
        std::pair{"coincident corners", geometry::Triangle3d{glm::dvec3(1, 2, 3), glm::dvec3(1, 2, 3), glm::dvec3(1, 2, 3)}});
    INFO("triangle: " << name);

    CHECK(geometry::compute_triangle_thickness(triangle) == 0.0);
}
