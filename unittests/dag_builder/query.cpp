#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "bake/query.h"
#include "geometry/geometry.h"

using bake::detail::Candidate;
using bake::detail::find_closest_point;
using bake::detail::find_line_hit;
using bake::detail::SourcePoint;

// The queries below share one skewed triangle, with no axis alignment and no right angle, so that no pair of weights can be swapped and no cross product transposed without the answer moving.
// Its corners are a = (0, 0, 0), b = (5, 0, 1) and c = (1, 4, 3), and the point at weights (0.2, 0.3, 0.5) is 0.3 * b + 0.5 * c = (2, 2, 1.8).
// Every line runs along (2, -1, 2) / 3, which is a unit direction whose triples are exact, so travelling three units moves a point by (2, -1, 2).

TEST_CASE("find_line_hit reports where the line crosses the candidate", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        // A decoy the line runs well past, so the reported index has to be found rather than assumed.
        {.triangle = {glm::dvec3(10, 10, 10), glm::dvec3(12, 10, 10), glm::dvec3(10, 12, 10)}, .normal_dot = 1.0},
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(5, 0, 1), glm::dvec3(1, 4, 3)}, .normal_dot = 1.0}};

    // Starting three units short of the crossing, at (2, 2, 1.8) - (2, -1, 2) = (0, 3, -0.2).
    const geometry::Line3d line{glm::dvec3(0, 3, -0.2), glm::normalize(glm::dvec3(2, -1, 2))};

    const std::optional<SourcePoint> found = find_line_hit(candidates, line, 100);

    REQUIRE(found);
    CHECK(found->candidate == 1);
    CHECK(found->barycentric.x == Catch::Approx(0.2));
    CHECK(found->barycentric.y == Catch::Approx(0.3));
    CHECK(found->barycentric.z == Catch::Approx(0.5));
    CHECK(found->distance_squared == Catch::Approx(9.0));
}

TEST_CASE("find_line_hit crosses a candidate behind the point the line passes through", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(5, 0, 1), glm::dvec3(1, 4, 3)}, .normal_dot = 1.0}};

    // Starting three units past the crossing, at (2, 2, 1.8) + (2, -1, 2) = (4, 1, 3.8), so the triangle stands behind the line's point.
    const geometry::Line3d line{glm::dvec3(4, 1, 3.8), glm::normalize(glm::dvec3(2, -1, 2))};

    const std::optional<SourcePoint> found = find_line_hit(candidates, line, 100);

    REQUIRE(found);
    CHECK(found->barycentric.x == Catch::Approx(0.2));
    CHECK(found->barycentric.y == Catch::Approx(0.3));
    CHECK(found->barycentric.z == Catch::Approx(0.5));
    CHECK(found->distance_squared == Catch::Approx(9.0));
}

TEST_CASE("find_line_hit takes the nearest crossing on either side", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        // The same triangle carried nine units along the line, so it is crossed six units ahead of the point.
        {.triangle = {glm::dvec3(6, -3, 6), glm::dvec3(11, -3, 7), glm::dvec3(7, 1, 9)}, .normal_dot = 1.0},
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(5, 0, 1), glm::dvec3(1, 4, 3)}, .normal_dot = 1.0}};

    // Three units past the crossing again, so the second candidate is three units behind and the first six ahead.
    const geometry::Line3d line{glm::dvec3(4, 1, 3.8), glm::normalize(glm::dvec3(2, -1, 2))};

    const std::optional<SourcePoint> found = find_line_hit(candidates, line, 100);

    REQUIRE(found);
    CHECK(found->candidate == 1);
    CHECK(found->distance_squared == Catch::Approx(9.0));
}

TEST_CASE("find_line_hit finds a crossing on an edge two candidates share", "[dag_builder][bake]") {
    // The triangle and its mirror across edge b-c, which together make a parallelogram closed by b + c - a = (6, 4, 4).
    const geometry::Triangle3d near_side = {glm::dvec3(0, 0, 0), glm::dvec3(5, 0, 1), glm::dvec3(1, 4, 3)};
    const geometry::Triangle3d far_side = {glm::dvec3(5, 0, 1), glm::dvec3(1, 4, 3), glm::dvec3(6, 4, 4)};

    // Aimed at the midpoint of the shared edge, (3, 2, 2), from three units short of it.
    const geometry::Line3d line{glm::dvec3(1, 3, 0), glm::normalize(glm::dvec3(2, -1, 2))};

    // A source mesh meets along its edges, so a line aimed at a seam has to land on the triangles either side of it rather than slip between them.
    SECTION("against the near side") {
        const std::vector<Candidate> candidates = {{.triangle = near_side, .normal_dot = 1.0}};

        const std::optional<SourcePoint> found = find_line_hit(candidates, line, 100);

        REQUIRE(found);
        CHECK(found->barycentric.x == Catch::Approx(0.0).margin(1e-12));
        CHECK(found->barycentric.y == Catch::Approx(0.5));
        CHECK(found->barycentric.z == Catch::Approx(0.5));
    }
    SECTION("against the far side") {
        const std::vector<Candidate> candidates = {{.triangle = far_side, .normal_dot = 1.0}};

        const std::optional<SourcePoint> found = find_line_hit(candidates, line, 100);

        REQUIRE(found);
        CHECK(found->barycentric.x == Catch::Approx(0.5));
        CHECK(found->barycentric.y == Catch::Approx(0.5));
        CHECK(found->barycentric.z == Catch::Approx(0.0).margin(1e-12));
    }
}

TEST_CASE("find_line_hit finds nothing where the line passes outside the candidate", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(5, 0, 1), glm::dvec3(1, 4, 3)}, .normal_dot = 1.0}};

    // Aimed at the point at weights (1.4, -0.2, -0.2), which lies in the triangle's plane out past the corner a, and started three units short of it.
    const geometry::Line3d line{glm::dvec3(-3.2, 0.2, -2.8), glm::normalize(glm::dvec3(2, -1, 2))};

    CHECK_FALSE(find_line_hit(candidates, line, 100));
}

TEST_CASE("find_line_hit ignores a candidate turned away from the target", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(5, 0, 1), glm::dvec3(1, 4, 3)}, .normal_dot = -1.0},
        // The same triangle carried three units along the line, so it is crossed three units beyond the nearer one.
        {.triangle = {glm::dvec3(2, -1, 2), glm::dvec3(7, -1, 3), glm::dvec3(3, 3, 5)}, .normal_dot = 1.0}};

    const geometry::Line3d line{glm::dvec3(0, 3, -0.2), glm::normalize(glm::dvec3(2, -1, 2))};

    const std::optional<SourcePoint> found = find_line_hit(candidates, line, 100);

    REQUIRE(found);
    CHECK(found->candidate == 1);
    CHECK(found->distance_squared == Catch::Approx(36.0));
}

TEST_CASE("find_line_hit finds nothing beyond max_distance", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(5, 0, 1), glm::dvec3(1, 4, 3)}, .normal_dot = 1.0}};

    const geometry::Line3d line{glm::dvec3(0, 3, -0.2), glm::normalize(glm::dvec3(2, -1, 2))};

    // The crossing stands three units along the line.
    CHECK(find_line_hit(candidates, line, 4.0));
    CHECK_FALSE(find_line_hit(candidates, line, 2.0));
}

// The closest point queries below stand on a triangle whose normal is that same (2, -1, 2) / 3, its corners a = (0, 0, 0), b = (1, 2, 0) and c = (0, 2, 1).
// The point at weights (0.2, 0.3, 0.5) is 0.3 * b + 0.5 * c = (0.3, 1.6, 0.5), so lifting it three units off the surface reaches (0.3, 1.6, 0.5) + (2, -1, 2) = (2.3, 0.6, 2.5).

TEST_CASE("find_closest_point reports the nearest point on the candidate", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(1, 2, 0), glm::dvec3(0, 2, 1)}, .normal_dot = 1.0}};

    const std::optional<SourcePoint> found = find_closest_point(candidates, glm::dvec3(2.3, 0.6, 2.5), std::nullopt, false);

    REQUIRE(found);
    CHECK(found->candidate == 0);
    CHECK(found->barycentric.x == Catch::Approx(0.2));
    CHECK(found->barycentric.y == Catch::Approx(0.3));
    CHECK(found->barycentric.z == Catch::Approx(0.5));
    CHECK(found->distance_squared == Catch::Approx(9.0));
}

TEST_CASE("find_closest_point clamps to the boundary when the perpendicular falls outside", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(1, 2, 0), glm::dvec3(0, 2, 1)}, .normal_dot = 1.0}};

    // A quarter along edge b-c stands (0.75, 2, 0.25), and (1, 4, 1) leaves the triangle from there across that edge, at a right angle to it and within its plane.
    const std::optional<SourcePoint> found = find_closest_point(candidates, glm::dvec3(1.75, 6, 1.25), std::nullopt, false);

    REQUIRE(found);
    CHECK(found->barycentric.x == Catch::Approx(0.0).margin(1e-12));
    CHECK(found->barycentric.y == Catch::Approx(0.75));
    CHECK(found->barycentric.z == Catch::Approx(0.25));
    // Nothing on the triangle stands nearer than that edge point, at (1, 4, 1) away.
    CHECK(found->distance_squared == Catch::Approx(18.0));
}

TEST_CASE("find_closest_point prefers a facing candidate to a nearer one turned away", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(1, 2, 0), glm::dvec3(0, 2, 1)}, .normal_dot = -1.0},
        // The same triangle carried three units down its normal, so it lies six units under the query rather than three.
        {.triangle = {glm::dvec3(-2, 1, -2), glm::dvec3(-1, 3, -2), glm::dvec3(-2, 3, -1)}, .normal_dot = 1.0}};

    const bool require_facing = GENERATE(false, true);
    const std::optional<SourcePoint> found = find_closest_point(candidates, glm::dvec3(2.3, 0.6, 2.5), std::nullopt, require_facing);

    REQUIRE(found);
    CHECK(found->candidate == 1);
    CHECK(found->distance_squared == Catch::Approx(36.0));
}

TEST_CASE("find_closest_point falls back to a candidate turned away unless facing is required", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(1, 2, 0), glm::dvec3(0, 2, 1)}, .normal_dot = -1.0}};
    const glm::dvec3 query(2.3, 0.6, 2.5);

    const std::optional<SourcePoint> found = find_closest_point(candidates, query, std::nullopt, false);

    REQUIRE(found);
    CHECK(found->candidate == 0);
    CHECK(found->distance_squared == Catch::Approx(9.0));

    CHECK_FALSE(find_closest_point(candidates, query, std::nullopt, true));
}

TEST_CASE("find_closest_point finds nothing beyond max_distance", "[dag_builder][bake]") {
    const std::vector<Candidate> candidates = {
        {.triangle = {glm::dvec3(0, 0, 0), glm::dvec3(1, 2, 0), glm::dvec3(0, 2, 1)}, .normal_dot = 1.0}};
    const glm::dvec3 query(2.3, 0.6, 2.5);

    // The nearest point stands three units under the query.
    CHECK(find_closest_point(candidates, query, 4.0, false));
    CHECK_FALSE(find_closest_point(candidates, query, 2.0, false));
    CHECK(find_closest_point(candidates, query, std::nullopt, false));
}

TEST_CASE("neither query finds anything among no candidates", "[dag_builder][bake]") {
    const std::span<const Candidate> none;
    const geometry::Line3d line{glm::dvec3(0, 3, -0.2), glm::normalize(glm::dvec3(2, -1, 2))};

    CHECK_FALSE(find_line_hit(none, line, 100));
    CHECK_FALSE(find_closest_point(none, glm::dvec3(2.3, 0.6, 2.5), std::nullopt, false));
}
