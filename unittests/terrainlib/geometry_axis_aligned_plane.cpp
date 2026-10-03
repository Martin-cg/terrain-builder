#include <algorithm>
#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "../catch2_helpers.h"

#include "geometry/AxisAlignedPlane.h"

TEST_CASE("SignedAxis keeps its index and sign", "[geometry][AxisAlignedPlane]") {
    for (glm::length_t index = 0; index < 3; index++) {
        for (const double sign : {-1.0, 1.0}) {
            CAPTURE(index, sign);
            const SignedAxis axis(index, sign);
            CHECK(axis.get_index() == index);
            CHECK(axis.get_sign() == sign);
        }
    }
}

TEST_CASE("SignedAxis equals only the same index and sign", "[geometry][AxisAlignedPlane]") {
    const std::vector<SignedAxis> axes = {
        SignedAxis(0, -1.0),
        SignedAxis(0, 1.0),
        SignedAxis(1, -1.0),
        SignedAxis(1, 1.0),
        SignedAxis(2, -1.0),
        SignedAxis(2, 1.0),
    };
    for (size_t i = 0; i < axes.size(); i++) {
        for (size_t j = 0; j < axes.size(); j++) {
            CAPTURE(i, j);
            CHECK((axes[i] == axes[j]) == (i == j));
        }
    }
}

TEST_CASE("geometry::compute_distance measures from an axis-aligned plane", "[geometry][AxisAlignedPlane]") {
    SECTION("normal along negative y") {
        // The plane y = 2.5, facing towards smaller y.
        const AxisAlignedPlane plane = {SignedAxis(1, -1.0), 2.5};
        CHECK_THAT(geometry::compute_distance(plane, glm::dvec3(0.1, 1.1, 7.3)), WithinEpsilon(1.4));
        CHECK_THAT(geometry::compute_distance(plane, glm::dvec3(-4.2, 3.7, 0.3)), WithinEpsilon(-1.2));
    }
    SECTION("normal along positive x") {
        // The plane x = 0.3, facing towards larger x.
        const AxisAlignedPlane plane = {SignedAxis(0, 1.0), -0.3};
        CHECK_THAT(geometry::compute_distance(plane, glm::dvec3(1.1, 5.0, -5.0)), WithinEpsilon(0.8));
        CHECK_THAT(geometry::compute_distance(plane, glm::dvec3(-0.2, -5.0, 5.0)), WithinEpsilon(-0.5));
    }
}

TEST_CASE("geometry::project_onto_plane sets the coordinate along the normal exactly", "[geometry][AxisAlignedPlane]") {
    SECTION("normal along positive z") {
        // The plane z = 0.3, where 0.7 - (0.7 - 0.3) != 0.3.
        const AxisAlignedPlane plane = {SignedAxis(2, 1.0), -0.3};
        CHECK(geometry::project_onto_plane(glm::dvec3(0.1, 1.1, 0.7), plane) == glm::dvec3(0.1, 1.1, 0.3));
    }
    SECTION("normal along negative x") {
        // The plane x = 1.1.
        const AxisAlignedPlane plane = {SignedAxis(0, -1.0), 1.1};
        CHECK(geometry::project_onto_plane(glm::dvec3(5.3, -0.2, 0.9), plane) == glm::dvec3(1.1, -0.2, 0.9));
    }
}

TEST_CASE("geometry::construct_face_planes faces every plane into the bounds", "[geometry][AxisAlignedPlane]") {
    const radix::geometry::Aabb3d bounds(glm::dvec3(0.1, -1.1, 2.0), glm::dvec3(1.3, 0.7, 2.9));
    const std::array<AxisAlignedPlane, 6> planes = geometry::construct_face_planes(bounds);

    const std::array<AxisAlignedPlane, 6> expected = {
        AxisAlignedPlane{SignedAxis(0, 1.0), -0.1},
        AxisAlignedPlane{SignedAxis(0, -1.0), 1.3},
        AxisAlignedPlane{SignedAxis(1, 1.0), 1.1},
        AxisAlignedPlane{SignedAxis(1, -1.0), 0.7},
        AxisAlignedPlane{SignedAxis(2, 1.0), -2.0},
        AxisAlignedPlane{SignedAxis(2, -1.0), 2.9},
    };
    CHECK(std::ranges::is_permutation(planes, expected));
}
