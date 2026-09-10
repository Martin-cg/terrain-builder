#include <cstdint>
#include <glm/glm.hpp>

#include "../catch2_helpers.h"
#include "numeric/quantize.h"

TEST_CASE("quantize_normalized spans the target's full range", "[quantize]") {
    CHECK(quantize_normalized<uint16_t>(0.0) == 0);
    CHECK(quantize_normalized<uint16_t>(1.0) == 65535);
    CHECK(quantize_normalized<uint8_t>(1.0) == 255);

    // The midpoint lands on the nearest step rather than truncating.
    CHECK(quantize_normalized<uint16_t>(0.5) == 32768);
}

TEST_CASE("quantize_normalized round-trips every representable step", "[quantize]") {
    for (uint32_t step = 0; step <= 255; step++) {
        const uint8_t quantized = static_cast<uint8_t>(step);
        REQUIRE(quantize_normalized<uint8_t>(dequantize_normalized(quantized)) == quantized);
    }
}

TEST_CASE("quantize_normalized maps every component of a vector", "[quantize]") {
    const glm::vec2 uv = {0.0f, 1.0f};

    CHECK(quantize_normalized<uint16_t>(uv) == glm::u16vec2{0, 65535});
    CHECK(dequantize_normalized(glm::u16vec2{0, 65535}) == glm::dvec2{0.0, 1.0});
}

TEST_CASE("round_by_style rounds in each direction", "[quantize]") {
    CHECK(round_by_style<std::round_toward_neg_infinity>(1.5) == 1.0);
    CHECK(round_by_style<std::round_toward_infinity>(1.5) == 2.0);
    CHECK(round_by_style<std::round_toward_zero>(1.5) == 1.0);
    CHECK(round_by_style<std::round_to_nearest>(1.5) == 2.0);

    CHECK(round_by_style<std::round_toward_neg_infinity>(-1.5) == -2.0);
    CHECK(round_by_style<std::round_toward_infinity>(-1.5) == -1.0);
    CHECK(round_by_style<std::round_toward_zero>(-1.5) == -1.0);
    CHECK(round_by_style<std::round_to_nearest>(-1.5) == -2.0);
}

TEST_CASE("round_by_style leaves whole values alone", "[quantize]") {
    CHECK(round_by_style<std::round_toward_neg_infinity>(4.0) == 4.0);
    CHECK(round_by_style<std::round_toward_infinity>(4.0) == 4.0);
    CHECK(round_by_style<std::round_toward_zero>(-4.0) == -4.0);
    CHECK(round_by_style<std::round_to_nearest>(-4.0) == -4.0);
}

TEST_CASE("round_by_style brackets the true value", "[quantize]") {
    for (int step = -2000; step <= 2000; step++) {
        const double x = static_cast<double>(step) * 0.37;

        const double floored = round_by_style<std::round_toward_neg_infinity>(x);
        const double ceiled = round_by_style<std::round_toward_infinity>(x);

        REQUIRE(floored <= x);
        REQUIRE(ceiled >= x);
        REQUIRE(ceiled - floored <= 1.0);
    }
}

TEST_CASE("quantize_floor scalar", "[quantize]") {
    CHECK(quantize_floor(3, 2) == 2);
    CHECK(quantize_floor(-2, 2) == -2);
    CHECK(quantize_floor(-1, 2) == -2);
    CHECK(quantize_floor(-3, 2) == -4);

    CHECK(quantize_floor(3.0, 2.0) == 2.0);
    CHECK(quantize_floor(-1.0, 2.0) == -2.0);
}

TEST_CASE("quantize_floor vector", "[quantize]") {
    CHECK(quantize_floor(glm::ivec2(3, -1), 2) == glm::ivec2(2, -2));
    CHECK(quantize_floor(glm::dvec2(3.0, -1.0), 2.0) == glm::dvec2(2.0, -2.0));
}

TEST_CASE("quantize_index scalar", "[quantize]") {
    CHECK(quantize_index(3, 2) == 1);
    CHECK(quantize_index(-2, 2) == -1);
    CHECK(quantize_index(-1, 2) == -1);
    CHECK(quantize_index(-3, 2) == -2);

    CHECK(quantize_index(3.0, 2.0) == 1);
    CHECK(quantize_index(-1.0, 2.0) == -1);
}

TEST_CASE("quantize_index vector", "[quantize]") {
    CHECK(quantize_index(glm::ivec2(3, -1), 2) == glm::i64vec2(1, -1));
    CHECK(quantize_index(glm::dvec2(3.0, -1.0), 2.0) == glm::i64vec2(1, -1));
}

// Neighbouring points must land in the same cell as long as they share a quantized corner.
TEST_CASE("quantize_index agrees with quantize_floor at earth-centered magnitudes", "[quantize]") {
    constexpr double epsilon = 1.28592041015625;
    constexpr double start = 4732875.8476;
    constexpr uint32_t sample_count = 1000;

    uint32_t disagreements = 0;
    for (uint32_t i = 0; i < sample_count; i++) {
        const double sample = start + i * 0.7;
        if (quantize_index(sample, epsilon) * epsilon != quantize_floor(sample, epsilon)) {
            disagreements++;
        }
    }

    CHECK(disagreements == 0);
}
