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