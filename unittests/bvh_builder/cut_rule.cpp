#include "../catch2_helpers.h"
#include "../result_helpers.h"

#include <cmath>

#include <glm/glm.hpp>

#include "Range.h"

#include "bvh/layout.h"
#include "view/Camera.h"
#include "view/CutRule.h"

TEST_CASE("crosses_cut tells whether the cut can pass between the errors of a header", "[view][CutRule]") {
    // An error at a distance shows as their ratio, so it fits if it is at most half the distance.
    const view::Camera camera = REQUIRE_VALUE(view::Camera::create({.eye = {0.0, 0.0, 0.0}, .forward = {1.0, 0.0, 0.0}, .up = {0.0, 0.0, 1.0}, .fov_y = 2.0 * std::atan(0.5), .aspect = 1.0, .z_near = 1.0}));
    const view::CutRule rule{.threshold = 0.5};

    SECTION("the error fits and the parent's does not") {
        CHECK(view::crosses_cut({.min_child_error = 4.0, .max_parent_error = 6.0, .bounds = {}}, RangeInclusive<double>(10.0), camera, rule));
    }
    SECTION("the error does not fit") {
        CHECK_FALSE(view::crosses_cut({.min_child_error = 6.0, .max_parent_error = 8.0, .bounds = {}}, RangeInclusive<double>(10.0), camera, rule));
    }
    SECTION("the parent's error fits as well") {
        CHECK_FALSE(view::crosses_cut({.min_child_error = 2.0, .max_parent_error = 4.0, .bounds = {}}, RangeInclusive<double>(10.0), camera, rule));
    }
    SECTION("the smallest error fits only from the farthest distance") {
        CHECK(view::crosses_cut({.min_child_error = 9.0, .max_parent_error = 12.0, .bounds = {}}, RangeInclusive<double>(10.0, 20.0), camera, rule));
    }
    SECTION("the largest parent error fails to fit only from the nearest distance") {
        CHECK(view::crosses_cut({.min_child_error = 2.0, .max_parent_error = 7.0, .bounds = {}}, RangeInclusive<double>(10.0, 20.0), camera, rule));
    }
    SECTION("closer than the near distance, where errors are measured at it") {
        CHECK(view::crosses_cut({.min_child_error = 0.4, .max_parent_error = 0.6, .bounds = {}}, RangeInclusive<double>(0.0), camera, rule));
    }
}
