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

}