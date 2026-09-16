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
    CHECK_THAT(glm::distance(frame->normal, radix::geometry::normal(triangle)), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
    CHECK_THAT(glm::length(frame->tangent), Catch::Matchers::WithinRel(1.0));
    CHECK_THAT(glm::length(frame->bitangent), Catch::Matchers::WithinRel(1.0));
    CHECK_THAT(glm::dot(frame->tangent, frame->bitangent), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
    CHECK_THAT(glm::dot(frame->tangent, frame->normal), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
    CHECK_THAT(glm::dot(frame->bitangent, frame->normal), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
}

TEST_CASE("PlaneFrame::from_triangle rejects a degenerate triangle", "[geometry][PlaneFrame]") {
    const radix::geometry::Triangle<3, double> collinear = {
        glm::dvec3(0.0, 0.0, 0.0),
        glm::dvec3(1.0, 1.0, 1.0),
        glm::dvec3(2.0, 2.0, 2.0),
    };
    CHECK_FALSE(PlaneFrame::from_triangle(collinear).has_value());

    const radix::geometry::Triangle<3, double> repeated = {
        glm::dvec3(3.0, 3.0, 3.0),
        glm::dvec3(3.0, 3.0, 3.0),
        glm::dvec3(5.0, 1.0, 0.0),
    };
    CHECK_FALSE(PlaneFrame::from_triangle(repeated).has_value());
}

TEST_CASE("CoordFrame maps to local coordinates and back", "[geometry][CoordFrame]") {
    const radix::geometry::Triangle<3, double> triangle = {
        glm::dvec3(1.0, -2.0, 3.0),
        glm::dvec3(4.0, 5.0, -1.0),
        glm::dvec3(-2.0, 3.0, 7.0),
    };
    const std::optional<PlaneFrame> frame = PlaneFrame::from_triangle(triangle);
    REQUIRE(frame.has_value());

    const glm::dvec3 position(7.0, -3.0, 2.0);
    CHECK_THAT(glm::distance(frame->to_world(frame->to_local(position)), position), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));

    // The origin sits at the local origin, and the corners lie in the plane.
    CHECK_THAT(glm::length(frame->to_local(frame->origin)), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
    for (const glm::dvec3 &corner : triangle) {
        CHECK_THAT(frame->distance_to(corner), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));
    }
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
    CHECK_THAT(frame->distance_to(raised), Catch::Matchers::WithinRel(2.5));
    CHECK_THAT(glm::length(frame->project(raised)), Catch::Matchers::WithinAbs(0.0, geometry::epsilon<double>()));

    const glm::dvec3 sunk = frame->origin - 1.5 * frame->normal;
    CHECK_THAT(frame->distance_to(sunk), Catch::Matchers::WithinRel(-1.5));
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
    CHECK_THAT(glm::distance(flat[0], flat[1]), Catch::Matchers::WithinRel(glm::distance(triangle[0], triangle[1])));
    CHECK_THAT(glm::distance(flat[1], flat[2]), Catch::Matchers::WithinRel(glm::distance(triangle[1], triangle[2])));
}
