#include <cstdint>
#include <limits>

#include <glm/glm.hpp>

#include "../../catch2_helpers.h"
#include "geometry/format.h"

namespace packer {

namespace {

constexpr geometry::QuantizedUv::value_type UV_MAX = std::numeric_limits<geometry::QuantizedUv::value_type>::max();
constexpr double UV_STEP = 1.0 / UV_MAX;

}

TEST_CASE("geometry::quantize_uv spans the unit square", "[geometry]") {
    CHECK(geometry::quantize_uv({0.0, 0.0}) == geometry::QuantizedUv{0, 0});
    CHECK(geometry::quantize_uv({1.0, 1.0}) == geometry::QuantizedUv{UV_MAX, UV_MAX});
}

TEST_CASE("geometry::quantize_uv round-trips within half a step", "[geometry]") {
    for (uint32_t step = 0; step <= 64; step++) {
        const double value = step / 64.0;
        const glm::dvec2 restored = geometry::dequantize_uv(geometry::quantize_uv({value, 1.0 - value}));

        REQUIRE_THAT(restored.x, Catch::Matchers::WithinAbs(value, UV_STEP / 2));
        REQUIRE_THAT(restored.y, Catch::Matchers::WithinAbs(1.0 - value, UV_STEP / 2));
    }
}

}