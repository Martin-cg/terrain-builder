#include <array>
#include <cmath>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "../catch2_helpers.h"

#include "geometry/geometry.h"

TEST_CASE("distance_to_segment clamps to the segment's ends", "[geometry]") {
    const radix::geometry::Edge<2, double> segment = {glm::dvec2(0.0, 0.0), glm::dvec2(4.0, 0.0)};

    CHECK(geometry::distance_to_segment(glm::dvec2(2.0, 3.0), segment) == Catch::Approx(3.0));
    CHECK(geometry::distance_to_segment(glm::dvec2(-3.0, 0.0), segment) == Catch::Approx(3.0));
    CHECK(geometry::distance_to_segment(glm::dvec2(9.0, 0.0), segment) == Catch::Approx(5.0));
    CHECK(geometry::distance_to_segment(glm::dvec2(1.0, 0.0), segment) == Catch::Approx(0.0));

    const radix::geometry::Edge<2, double> degenerate = {glm::dvec2(1.0, 1.0), glm::dvec2(1.0, 1.0)};
    CHECK(geometry::distance_to_segment(glm::dvec2(1.0, 4.0), degenerate) == Catch::Approx(3.0));
}

TEST_CASE("distance_to_triangle is zero inside and grows outside", "[geometry]") {
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

TEST_CASE("distance_to_triangle accepts either winding", "[geometry]") {
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

TEST_CASE("triangles_overlap counts touching as overlapping", "[geometry]") {
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

TEST_CASE("lerp keeps a coordinate both ends share exact", "[geometry]") {
    // glm::mix computes 0.1 * (1 - 0.3) + 0.1 * 0.3, which rounds to 0.09999999999999999.
    const glm::dvec2 interpolated = geometry::lerp(glm::dvec2(1.0, 0.1), glm::dvec2(3.0, 0.1), 0.3);
    CHECK_THAT(interpolated.x, WithinEpsilon(1.6));
    CHECK(interpolated.y == 0.1);
}

TEST_CASE("lerp is exact at both ends", "[geometry]") {
    // 0.7 + 1 * (0.1 - 0.7) rounds to 0.09999999999999998.
    const glm::dvec2 from(0.7, 1.1);
    const glm::dvec2 to(0.1, 0.3);
    CHECK(geometry::lerp(from, to, 0.0) == from);
    CHECK(geometry::lerp(from, to, 1.0) == to);
}

TEST_CASE("compute_longest_edge picks the longest of the three edges", "[geometry]") {
    const geometry::Triangle3d triangle = {glm::dvec3(0, 0, 0), glm::dvec3(3, 0, 0), glm::dvec3(0, 4, 0)};

    CHECK(geometry::compute_longest_edge(triangle) == 5.0);
}

TEST_CASE("compute_triangle_thickness is the smallest altitude", "[geometry]") {
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

TEST_CASE("compute_triangle_thickness is zero for degenerate triangles", "[geometry]") {
    const auto [name, triangle] = GENERATE(
        std::pair{"collinear corners", geometry::Triangle3d{glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(3, 0, 0)}},
        std::pair{"repeated corner", geometry::Triangle3d{glm::dvec3(0, 0, 0), glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0)}},
        std::pair{"coincident corners", geometry::Triangle3d{glm::dvec3(1, 2, 3), glm::dvec3(1, 2, 3), glm::dvec3(1, 2, 3)}});
    INFO("triangle: " << name);

    CHECK(geometry::compute_triangle_thickness(triangle) == 0.0);
}

TEST_CASE("is_collinear finds points on one line at an epsilon of 0 where the edges round", "[geometry]") {
    // All three lie on the line through the origin along (3, 5, 7), but b - a and c - a round in different proportions.
    const glm::dvec3 a = glm::dvec3(3, 5, 7) * 0x1p-53;
    const glm::dvec3 b(3, 5, 7);
    const glm::dvec3 c(6, 10, 14);
    CHECK(geometry::is_collinear(a, b, c, 0.0));
}

TEST_CASE("is_collinear rejects a point one ulp off the line at an epsilon of 0", "[geometry]") {
    const glm::dvec3 a = glm::dvec3(3, 5, 7) * 0x1p-53;
    const glm::dvec3 b(3, 5, 7);
    const glm::dvec3 c(6, 10, std::nextafter(14.0, 15.0));
    CHECK_FALSE(geometry::is_collinear(a, b, c, 0.0));
}

TEST_CASE("is_collinear rejects a sliver whose area rounds to zero at an epsilon of 0", "[geometry]") {
    const double just_below_one = std::nextafter(1.0, 0.0);
    const double half_gap = (1.0 - just_below_one) / 2;
    const glm::dvec3 a(just_below_one, half_gap, 0);
    const glm::dvec3 b(1, half_gap, 0);
    const glm::dvec3 c(1, 1, 0);
    CHECK_FALSE(geometry::is_collinear(a, b, c, 0.0));
}

TEST_CASE("is_collinear counts coincident points as collinear", "[geometry]") {
    const glm::dvec3 a(1, 2, 3);
    const glm::dvec3 c(4, -5, 6);
    CHECK(geometry::is_collinear(a, a, c, 0.0));
    CHECK(geometry::is_collinear(a, c, c, 0.0));
    CHECK(geometry::is_collinear(a, a, a, 0.0));
}

TEST_CASE("is_collinear compares epsilon to the distance of the nearest point from the line through the other two", "[geometry]") {
    // The middle point lies 1e-6 off the line through the outer two, and each outer point lies about 2e-6 off the line through the others.
    const glm::dvec2 left(0, 0);
    const glm::dvec2 middle(0.5, 1e-6);
    const glm::dvec2 right(1, 0);
    const auto [a, b, c] = GENERATE_COPY(
        std::array{left, middle, right},
        std::array{middle, right, left},
        std::array{right, left, middle});
    CAPTURE(a, b, c);
    CHECK(geometry::is_collinear(a, b, c, 1.5e-6));
    CHECK_FALSE(geometry::is_collinear(a, b, c, 0.5e-6));
}
