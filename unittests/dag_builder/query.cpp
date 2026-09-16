#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "bake/query.h"
#include "geometry/geometry.h"

using bake::detail::Candidate;
using bake::detail::find_closest_point;
using bake::detail::SourcePoint;

// The closest point queries below stand on a triangle whose normal is (2, -1, 2) / 3, its corners a = (0, 0, 0), b = (1, 2, 0) and c = (0, 2, 1).
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
