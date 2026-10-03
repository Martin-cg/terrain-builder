#include <cstdint>
#include <optional>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "../catch2_helpers.h"

#include "geometry/PlaneFrame.h"
#include "geometry/geometry.h"

TEST_CASE("PlaneFrame::from_triangle spans the triangle's plane", "[geometry][PlaneFrame]") {
    const radix::geometry::Triangle<3, double> triangle = {
        glm::dvec3(1.0, -2.0, 3.0),
        glm::dvec3(4.0, 5.0, -1.0),
        glm::dvec3(-2.0, 3.0, 7.0),
    };
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(triangle);

    REQUIRE(frame.has_value());
    // The edges from the first corner are (3, 7, -4) and (-3, 5, 4), whose cross product is (48, 0, 36).
    CHECK_THAT(glm::distance(frame->normal, glm::dvec3(0.8, 0.0, 0.6)), WithinEpsilon(0.0));
    CHECK_THAT(glm::length(frame->tangent), WithinEpsilon(1.0));
    CHECK_THAT(glm::length(frame->bitangent), WithinEpsilon(1.0));
    CHECK_THAT(glm::dot(frame->tangent, frame->bitangent), WithinEpsilon(0.0));
    CHECK_THAT(glm::dot(frame->tangent, frame->normal), WithinEpsilon(0.0));
    CHECK_THAT(glm::dot(frame->bitangent, frame->normal), WithinEpsilon(0.0));
    for (const glm::dvec3 &corner : triangle) {
        CHECK_THAT(frame->distance_to(corner), WithinEpsilon(0.0));
    }
}

TEST_CASE("PlaneFrame::from_triangle rejects a triangle thinner than epsilon", "[geometry][PlaneFrame]") {
    SECTION("collinear corners") {
        const radix::geometry::Triangle<3, double> collinear = {
            glm::dvec3(0.0, 0.0, 0.0),
            glm::dvec3(1.0, 1.0, 1.0),
            glm::dvec3(2.0, 2.0, 2.0),
        };
        CHECK_FALSE(PlaneFrame::from_triangle(collinear).has_value());
    }
    SECTION("repeated corner") {
        const radix::geometry::Triangle<3, double> repeated = {
            glm::dvec3(3.0, 3.0, 3.0),
            glm::dvec3(3.0, 3.0, 3.0),
            glm::dvec3(5.0, 1.0, 0.0),
        };
        CHECK_FALSE(PlaneFrame::from_triangle(repeated).has_value());
    }
    SECTION("flat sliver without a short edge") {
        const radix::geometry::Triangle<3, double> sliver = {
            glm::dvec3(0.0, 0.0, 0.0),
            glm::dvec3(1000.0, 0.0, 0.0),
            glm::dvec3(500.0, 1e-13, 0.0),
        };
        CHECK_FALSE(PlaneFrame::from_triangle(sliver).has_value());
    }
}

TEST_CASE("PlaneFrame::from_triangle rejects a needle wherever its short edge lies", "[geometry][PlaneFrame]") {
    // The area is well above epsilon, but the short edge makes the triangle thinner than it.
    const radix::geometry::Triangle<3, double> corners = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1e-13, 0.0, 0.0),
        glm::dvec3(0.0, 1000.0, 0.0),
    };
    const uint32_t first = GENERATE(0u, 1u, 2u);
    const radix::geometry::Triangle<3, double> needle = {corners[first], corners[(first + 1) % 3], corners[(first + 2) % 3]};
    INFO("first corner: " << first);

    CHECK_FALSE(PlaneFrame::from_triangle(needle).has_value());
}

TEST_CASE("PlaneFrame separates the in-plane position from the height", "[geometry][PlaneFrame]") {
    const radix::geometry::Triangle<3, double> triangle = {
        glm::dvec3(1.0, -2.0, 3.0),
        glm::dvec3(4.0, 5.0, -1.0),
        glm::dvec3(-2.0, 3.0, 7.0),
    };
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(triangle);
    REQUIRE(frame.has_value());

    const glm::dvec3 raised = frame->origin + 2.5 * frame->normal;
    CHECK_THAT(frame->distance_to(raised), WithinEpsilon(2.5));
    CHECK_THAT(glm::length(frame->project(raised)), WithinEpsilon(0.0));

    const glm::dvec3 sunk = frame->origin - 1.5 * frame->normal;
    CHECK_THAT(frame->distance_to(sunk), WithinEpsilon(-1.5));
}

TEST_CASE("PlaneFrame::flatten lays a triangle out counter-clockwise", "[geometry][PlaneFrame]") {
    const radix::geometry::Triangle<3, double> triangle = {
        glm::dvec3(1.0, -2.0, 3.0),
        glm::dvec3(4.0, 5.0, -1.0),
        glm::dvec3(-2.0, 3.0, 7.0),
    };
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(triangle);
    REQUIRE(frame.has_value());

    const radix::geometry::Triangle<2, double> flat = frame->flatten(triangle);
    CHECK(geometry::cross(flat[1] - flat[0], flat[2] - flat[0]) > 0.0);
    // Flattening preserves lengths, since the frame is orthonormal and the triangle lies in its plane.
    CHECK_THAT(glm::distance(flat[0], flat[1]), WithinEpsilon(glm::distance(triangle[0], triangle[1])));
    CHECK_THAT(glm::distance(flat[1], flat[2]), WithinEpsilon(glm::distance(triangle[1], triangle[2])));
}