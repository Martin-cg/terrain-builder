#include <vector>

#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "quantize.h"

namespace packer {

TEST_CASE("quantize_point lands on the global grid", "[quantize]") {
    CHECK(quantize_point({0.0, 0.0, 0.0}) == QuantizedPoint{0, 0, 0});
    CHECK(quantize_point({GRID_STEP, 2 * GRID_STEP, -3 * GRID_STEP}) == QuantizedPoint{1, 2, -3});
}

TEST_CASE("quantize_point round-trips within half a step", "[quantize]") {
    const std::vector<glm::dvec3> points = {
        {0.0, 0.0, 0.0},
        {1.5, -2.25, 1000.125},
        {-6378137.0, 6378137.0, 0.5},
        {0.0004, -0.0004, 0.0009},
    };

    for (const glm::dvec3 &point : points) {
        const glm::dvec3 restored = dequantize_point(quantize_point(point));

        REQUIRE_THAT(restored.x, Catch::Matchers::WithinAbs(point.x, GRID_STEP / 2));
        REQUIRE_THAT(restored.y, Catch::Matchers::WithinAbs(point.y, GRID_STEP / 2));
        REQUIRE_THAT(restored.z, Catch::Matchers::WithinAbs(point.z, GRID_STEP / 2));
    }
}

TEST_CASE("quantize_bounds contains the bounds it quantizes", "[quantize]") {
    const std::vector<radix::geometry::Aabb3d> all_bounds = {
        {.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}},
        {.min = {0.0004, -0.0004, 0.0009}, .max = {1.5, -0.0001, 1000.125}},
        {.min = {-6378137.3, -2.25, 0.5}, .max = {6378137.7, 2.25, 0.5}},
    };

    for (const radix::geometry::Aabb3d &bounds : all_bounds) {
        const GridBox quantized = quantize_bounds(bounds);
        const radix::geometry::Aabb3d restored{.min = dequantize_point(quantized.min), .max = dequantize_point(quantized.max)};

        REQUIRE(restored.contains_inclusive(bounds.min));
        REQUIRE(restored.contains_inclusive(bounds.max));
    }
}

TEST_CASE("quantize_bounds keeps bounds on the grid unchanged", "[quantize]") {
    const radix::geometry::Aabb3d bounds{.min = {-3 * GRID_STEP, 0.0, 5.0}, .max = {7 * GRID_STEP, 2.0, 1000.0}};
    const FixedPoint::repr_t unit = FixedPoint::scale;

    CHECK(quantize_bounds(bounds) == GridBox{.min = {-3, 0, 5 * unit}, .max = {7, 2 * unit, 1000 * unit}});
}

}
