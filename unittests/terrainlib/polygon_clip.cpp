#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "../catch2_helpers.h"

#include "containers/FixedVector.h"
#include "geometry/geometry.h"
#include "polygon/clip.h"

namespace {

using geometry::Triangle2d;
using Plane = radix::geometry::Plane<double>;
using Edge2d = radix::geometry::Edge<2, double>;

// Checks that `actual` is the polygon `expected`, starting at any of its vertices.
template <typename Actual>
void check_polygon(const Actual &actual, const std::vector<std::ranges::range_value_t<Actual>> &expected) {
    CAPTURE(actual, expected);
    REQUIRE(std::ranges::size(actual) == expected.size());
    if (expected.empty()) {
        return;
    }

    std::vector<std::ranges::range_value_t<Actual>> rotated(std::ranges::begin(actual), std::ranges::end(actual));
    const auto start = std::ranges::find(rotated, expected.front());
    REQUIRE(start != rotated.end());
    std::ranges::rotate(rotated, start);
    CHECK(rotated == expected);
}

Triangle2d make_triangle(const glm::dvec2 &a, const glm::dvec2 &b, const glm::dvec2 &c) {
    return {a, b, c};
}

std::vector<Triangle2d> clip(const Triangle2d &subject, const Triangle2d &clip_triangle) {
    std::vector<Triangle2d> out;
    polygon::clip_into_triangles(subject, clip_triangle, out);
    return out;
}

double area_of(const Triangle2d &triangle) {
    return std::abs(geometry::cross(triangle[1] - triangle[0], triangle[2] - triangle[0])) / 2.0;
}

double total_area(const std::vector<Triangle2d> &triangles) {
    double area = 0.0;
    for (const Triangle2d &triangle : triangles) {
        area += area_of(triangle);
    }
    return area;
}

// The lower left half of the unit square.
const Triangle2d unit = make_triangle({0, 0}, {1, 0}, {0, 1});

} // namespace

TEST_CASE("clip_triangle keeps a triangle contained in the clip", "[polygon][clip]") {
    const Triangle2d big = make_triangle({-1, -1}, {3, -1}, {-1, 3});

    const std::vector<Triangle2d> pieces = clip(unit, big);

    REQUIRE(pieces.size() == 1);
    CHECK(total_area(pieces) == Catch::Approx(area_of(unit)));
}

TEST_CASE("clip_triangle drops a triangle outside the clip", "[polygon][clip]") {
    const Triangle2d far_away = make_triangle({5, 5}, {6, 5}, {5, 6});

    CHECK(clip(unit, far_away).empty());
}

TEST_CASE("clip_triangle drops triangles meeting in a point or a segment", "[polygon][clip]") {
    // Shares only the corner at (1, 0).
    const Triangle2d at_corner = make_triangle({1, 0}, {2, 0}, {2, 1});
    CHECK(clip(unit, at_corner).empty());

    // Shares the whole edge from (0, 0) to (1, 0), lying on the other side of it.
    const Triangle2d below_edge = make_triangle({0, 0}, {1, 0}, {0, -1});
    CHECK(clip(unit, below_edge).empty());
}

TEST_CASE("clip_triangle halves a triangle cut by the clip", "[polygon][clip]") {
    // Covers x <= 0.5, reaching far enough that its other two edges cut nothing.
    const Triangle2d left_half = make_triangle({0.5, -10}, {0.5, 10}, {-20, 0});

    const std::vector<Triangle2d> pieces = clip(unit, left_half);

    REQUIRE(!pieces.empty());
    // The unit triangle left of x = 0.5 is a trapezoid of area 3/8.
    CHECK(total_area(pieces) == Catch::Approx(0.375));
}

TEST_CASE("clip_triangle produces a hexagon for a star overlap", "[polygon][clip]") {
    // Two triangles in opposite orientation overlapping in a hexagon.
    const Triangle2d up = make_triangle({0, 0}, {6, 0}, {3, 6});
    const Triangle2d down = make_triangle({0, 4}, {6, 4}, {3, -2});

    const std::vector<Triangle2d> pieces = clip(up, down);

    // A hexagon fans into four triangles.
    REQUIRE(pieces.size() == 4);
    // Corners (2,0) (4,0) (5,2) (4,4) (2,4) (1,2).
    CHECK(total_area(pieces) == Catch::Approx(12.0));
}

TEST_CASE("clip_triangle is insensitive to winding", "[polygon][clip]") {
    const Triangle2d reversed_clip = make_triangle({-1, -1}, {-1, 3}, {3, -1});
    const Triangle2d reversed_subject = {unit[0], unit[2], unit[1]};

    CHECK(total_area(clip(unit, reversed_clip)) == Catch::Approx(area_of(unit)));
    CHECK(total_area(clip(reversed_subject, reversed_clip)) == Catch::Approx(area_of(unit)));
}

TEST_CASE("clip_triangle covers the overlap without counting it twice", "[polygon][clip]") {
    // A generic pair with no shared corners or parallel edges.
    const Triangle2d subject = make_triangle({0, 0}, {4, 1}, {1, 4});
    const Triangle2d clip_triangle = make_triangle({3, 3}, {-1, 2}, {2, -1});

    const std::vector<Triangle2d> pieces = clip(subject, clip_triangle);
    REQUIRE(pieces.size() >= 1);

    // The fan tiles the overlap, so sampling it must hit exactly one piece per point.
    for (const Triangle2d &piece : pieces) {
        const glm::dvec2 centre = (piece[0] + piece[1] + piece[2]) / 3.0;
        REQUIRE(geometry::distance_to_triangle(centre, subject) == 0.0);
        REQUIRE(geometry::distance_to_triangle(centre, clip_triangle) == 0.0);

        uint32_t containing = 0;
        for (const Triangle2d &other : pieces) {
            containing += geometry::distance_to_triangle(centre, other) == 0.0 ? 1 : 0;
        }
        REQUIRE(containing == 1);
    }
}

TEST_CASE("clip_triangle drops a degenerate subject", "[polygon][clip]") {
    const Triangle2d collinear = make_triangle({0, 0}, {1, 1}, {2, 2});
    const Triangle2d big = make_triangle({-1, -1}, {3, -1}, {-1, 3});

    CHECK(total_area(clip(collinear, big)) == Catch::Approx(0.0));
    CHECK(clip(unit, collinear).empty());
}

TEST_CASE("polygon::clip keeps the part of a triangle on the side of a plane its normal points to", "[polygon][clip]") {
    const FixedVector<glm::dvec3, 4> triangle = {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(1, 1, 0)};

    SECTION("a triangle on the kept side is unchanged") {
        const Plane keep_x_below_2 = {glm::dvec3(-1, 0, 0), 2};
        check_polygon(polygon::clip(triangle, keep_x_below_2), {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(1, 1, 0)});
    }

    SECTION("clipping one vertex leaves a quad") {
        const Plane keep_x_above_half = {glm::dvec3(1, 0, 0), -0.5};
        check_polygon(polygon::clip(triangle, keep_x_above_half), {glm::dvec3(0.5, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(1, 1, 0), glm::dvec3(0.5, 0.5, 0)});
    }

    SECTION("clipping two vertices leaves a triangle") {
        const Plane keep_x_below_half = {glm::dvec3(-1, 0, 0), 0.5};
        check_polygon(polygon::clip(triangle, keep_x_below_half), {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0)});
    }

    SECTION("a triangle on the other side is discarded") {
        const Plane keep_x_below_minus_1 = {glm::dvec3(-1, 0, 0), -1};
        check_polygon(polygon::clip(triangle, keep_x_below_minus_1), {});
    }
}

TEST_CASE("polygon::clip keeps the part of a triangle on the side of every plane", "[polygon][clip]") {
    const FixedVector<glm::dvec3, 5> triangle = {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(0, 1, 0)};
    const std::array<Plane, 2> keep_x_and_y_below_half = {
        Plane{glm::dvec3(-1, 0, 0), 0.5},
        Plane{glm::dvec3(0, -1, 0), 0.5},
    };

    check_polygon(polygon::clip(triangle, std::span<const Plane>(keep_x_and_y_below_half)), {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 0.5, 0), glm::dvec3(0, 0.5, 0)});
}

TEST_CASE("polygon::clip keeps the part of a 2d triangle left of an edge", "[polygon][clip]") {
    const FixedVector<glm::dvec2, 4> triangle = {glm::dvec2(0, 0), glm::dvec2(1, 0), glm::dvec2(1, 1)};
    const Edge2d upwards_at_x_half = {glm::dvec2(0.5, 0), glm::dvec2(0.5, 1)};

    check_polygon(polygon::clip(triangle, upwards_at_x_half), {glm::dvec2(0, 0), glm::dvec2(0.5, 0), glm::dvec2(0.5, 0.5)});
}

TEST_CASE("polygon::clip keeps the part of a quad on the side of a plane its normal points to", "[polygon][clip]") {
    const FixedVector<glm::dvec3, 5> quad = {glm::dvec3(0, 0, 0), glm::dvec3(1, 0, 0), glm::dvec3(1, 1, 0), glm::dvec3(0, 1, 0)};
    const Plane keep_x_below_half = {glm::dvec3(-1, 0, 0), 0.5};

    check_polygon(polygon::clip(quad, keep_x_below_half), {glm::dvec3(0, 0, 0), glm::dvec3(0.5, 0, 0), glm::dvec3(0.5, 1, 0), glm::dvec3(0, 1, 0)});
}
