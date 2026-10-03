#include "../catch2_helpers.h"

#include "numeric/ExactSum.h"

TEST_CASE("ExactSum is zero without any values", "[numeric][ExactSum]") {
    const ExactSum<double, 1> sum;
    CHECK(sum.is_zero());
    CHECK(sum.estimate() == 0.0);
}

TEST_CASE("ExactSum::add keeps what a rounded sum loses", "[numeric][ExactSum]") {
    // 1 + 2^-60 rounds to 1, so a rounded sum ends at 0.
    ExactSum<double, 3> sum;
    sum.add(1.0);
    sum.add(0x1p-60);
    sum.add(-1.0);
    CHECK_FALSE(sum.is_zero());
    CHECK(sum.estimate() == 0x1p-60);
}

TEST_CASE("ExactSum::is_zero finds values that cancel", "[numeric][ExactSum]") {
    // 1e16 + 1 rounds to 1e16, so a rounded sum ends at -1.
    ExactSum<double, 4> sum;
    sum.add(1e16);
    sum.add(1.0);
    sum.add(-1e16);
    sum.add(-1.0);
    CHECK(sum.is_zero());
    CHECK(sum.estimate() == 0.0);
}

TEST_CASE("ExactSum::add_product keeps what the rounded product loses", "[numeric][ExactSum]") {
    // (1 + 2^-30)^2 is 1 + 2^-29 + 2^-60, which rounds to 1 + 2^-29.
    const double factor = 1.0 + 0x1p-30;
    ExactSum<double, 5> sum;
    sum.add_product(factor, factor);
    sum.add(-1.0);
    sum.add(-0x1p-29);
    CHECK_FALSE(sum.is_zero());
    CHECK(sum.estimate() == 0x1p-60);

    sum.add(-0x1p-60);
    CHECK(sum.is_zero());
}
