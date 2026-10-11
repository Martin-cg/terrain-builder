#include "../catch2_helpers.h"

#include <limits>

#include <glm/glm.hpp>
#include <radix/geometry.h>

#include "build/merge_cost.h"
#include "bvh/layout.h"

TEST_CASE("extend_by_error adds the scaled error as a fourth axis", "[build][merge_cost]") {
    const bvh::NodeHeader header{.min_child_error = 1.0, .max_parent_error = 3.0, .bounds = {glm::dvec3{0.0, 0.0, 0.0}, glm::dvec3{1.0, 2.0, 3.0}}};

    const radix::geometry::Aabb<4, double> extended = build::extend_by_error(header, 2.0);
    CHECK(extended.min == glm::dvec4{0.0, 0.0, 0.0, 2.0});
    CHECK(extended.max == glm::dvec4{1.0, 2.0, 3.0, 6.0});
}

TEST_CASE("extend_by_error ignores an unbounded parent error", "[build][merge_cost]") {
    const bvh::NodeHeader header{.min_child_error = 1.0, .max_parent_error = std::numeric_limits<double>::infinity(), .bounds = {glm::dvec3{0.0, 0.0, 0.0}, glm::dvec3{1.0, 2.0, 3.0}}};

    // The lower end of the error interval stands in for it.
    const radix::geometry::Aabb<4, double> extended = build::extend_by_error(header, 2.0);
    CHECK(extended.min == glm::dvec4{0.0, 0.0, 0.0, 2.0});
    CHECK(extended.max == glm::dvec4{1.0, 2.0, 3.0, 2.0});
}

TEST_CASE("compute_merge_cost costs the box around both", "[build][merge_cost]") {
    const radix::geometry::Aabb<4, double> left{glm::dvec4{0.0, 0.0, 0.0, 0.0}, glm::dvec4{1.0, 1.0, 3.0, 1.0}};
    const radix::geometry::Aabb<4, double> right{glm::dvec4{0.0, 1.0, 0.0, 3.0}, glm::dvec4{1.0, 2.0, 1.0, 4.0}};

    // The box around both has sizes 1, 2, 3 and 4.
    // Its cost is twice the sum of the six products of two sizes, 2 * (2 + 3 + 4 + 6 + 8 + 12).
    CHECK_THAT(build::compute_merge_cost(left, right), WithinEpsilon(70.0));
}
