#include "../catch2_helpers.h"

#include "numeric/int_math.h"

TEST_CASE("int_div_round rounds half away from zero", "[int_math]") {
    // Positive half rounds up
    CHECK(int_div_round(1, 2) == 1);
    CHECK(int_div_round(3, 2) == 2);

    // Negative half rounds away from zero (down in absolute value)
    CHECK(int_div_round(-1, 2) == -1);
    CHECK(int_div_round(-3, 2) == -2);

    // Below half rounds down
    CHECK(int_div_round(7, 3) == 2);
    CHECK(int_div_round(-7, 3) == -2);

    // Above half rounds up
    CHECK(int_div_round(7, 4) == 2);
    CHECK(int_div_round(-7, 4) == -2);

    // Negative denominator normalizes correctly
    CHECK(int_div_round(7, -4) == int_div_round(-7, 4));
    CHECK(int_div_round(-7, -4) == int_div_round(7, 4));
}

TEST_CASE("int_div_floor rounds toward negative infinity", "[int_math]") {
    CHECK(int_div_floor(7, 2) == 3);
    CHECK(int_div_floor(-7, 2) == -4);
    CHECK(int_div_floor(7, -2) == -4);
    CHECK(int_div_floor(-7, -2) == 3);

    // Exact division is its own floor in every sign combination.
    CHECK(int_div_floor(-8, 2) == -4);
    CHECK(int_div_floor(8, -2) == -4);
}

TEST_CASE("int_div_by_style rounds in the requested direction", "[int_math]") {
    CHECK(int_div_by_style<std::round_toward_neg_infinity>(-7, 2) == -4);
    CHECK(int_div_by_style<std::round_toward_infinity>(-7, 2) == -3);
    CHECK(int_div_by_style<std::round_toward_zero>(-7, 2) == -3);
    CHECK(int_div_by_style<std::round_to_nearest>(-7, 2) == -4);

    CHECK(int_div_by_style<std::round_toward_neg_infinity>(7, 2) == 3);
    CHECK(int_div_by_style<std::round_toward_infinity>(7, 2) == 4);
    CHECK(int_div_by_style<std::round_toward_zero>(7, 2) == 3);
    CHECK(int_div_by_style<std::round_to_nearest>(7, 2) == 4);
}

TEST_CASE("bytes_for_bits rounds up to whole bytes", "[int_math]") {
    CHECK(bytes_for_bits(0) == 0);
    CHECK(bytes_for_bits(1) == 1);
    CHECK(bytes_for_bits(8) == 1);
    CHECK(bytes_for_bits(9) == 2);
    CHECK(bytes_for_bits(64) == 8);
    CHECK(bytes_for_bits(65) == 9);
}

TEST_CASE("int_width_for_bits rounds up to an integer type width", "[int_math]") {
    // Anything below a byte still occupies one.
    CHECK(int_width_for_bits(0) == 8);
    CHECK(int_width_for_bits(1) == 8);
    CHECK(int_width_for_bits(8) == 8);

    // A width is exact, one bit past it steps to the next type.
    CHECK(int_width_for_bits(9) == 16);
    CHECK(int_width_for_bits(16) == 16);
    CHECK(int_width_for_bits(17) == 32);
    CHECK(int_width_for_bits(32) == 32);
    CHECK(int_width_for_bits(33) == 64);
    CHECK(int_width_for_bits(64) == 64);

    // Widths past the built-in types stay powers of two, which the wide integer fallback needs.
    CHECK(int_width_for_bits(65) == 128);
    CHECK(int_width_for_bits(128) == 128);

    // Byte counts that are not powers of two round up rather than landing between types.
    CHECK(int_width_for_bits(24) == 32);
    CHECK(int_width_for_bits(48) == 64);
}

TEST_CASE("max_for_bits spans the width it is given", "[int_math]") {
    CHECK(max_for_bits(0) == 0);
    CHECK(max_for_bits(1) == 1);
    CHECK(max_for_bits(8) == 255);
    CHECK(max_for_bits(16) == 65535);

    // The full width of the type, where forming it by shifting would be undefined.
    CHECK(max_for_bits(64) == std::numeric_limits<uint64_t>::max());
    CHECK(max_for_bits<uint8_t>(8) == std::numeric_limits<uint8_t>::max());
    CHECK(max_for_bits<uint32_t>(32) == std::numeric_limits<uint32_t>::max());
}

TEST_CASE("int_div_round does not overflow on a near-maximal denominator", "[int_math]") {
    constexpr std::int64_t maximum = std::numeric_limits<std::int64_t>::max();

    // Doubling this remainder would reach 2^64 - 2 and wrap.
    CHECK(int_div_round(maximum - 1, maximum) == 1);
    CHECK(int_div_round(maximum, maximum) == 1);

    // An even denominator, so half of it is representable and lands exactly on the rounding boundary.
    constexpr std::int64_t even = maximum - 1;
    CHECK(int_div_round(even / 2, even) == 1);
    CHECK(int_div_round(even / 2 - 1, even) == 0);
}