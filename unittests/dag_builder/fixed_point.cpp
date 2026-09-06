#include <glm/glm.hpp>

#include "../catch2_helpers.h"

#include "fixed.h"
#include "fixed_point.h"

TEST_CASE("fixed::Q mul performs fixed-point multiplication with rounding", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<16>;

    // Small integer multiplication preserves scale
    const auto a_val = Q::from_integer(2).value;
    const auto b_val = Q::from_integer(3).value;
    const auto expected = Q::from_integer(6).value;
    CHECK(Q::mul(a_val, b_val) == expected);

    // Half-LSB case: multiplying fixed-point 0.5 (32768) by itself
    const auto half = Q::from_real(0.5).value;
    const auto quarter = Q::mul(half, half);
    CHECK(quarter == Q::from_real(0.25).value);

    // Very small multiplication rounds to zero
    CHECK(Q::mul(1, 1) == 0);

    // Inverse relationship with division for values at scale
    const auto a = Q::scale;
    const auto b = Q::scale * 3;
    const auto product = Q::mul(a, b);
    const auto divided = Q::div(product, a);
    CHECK(divided == b);
}

TEST_CASE("fixed::Q div performs fixed-point division with rounding", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<16>;

    // Division is inverse of multiplication for exactly representable values
    const auto a_val = Q::from_integer(6).value;
    const auto b_val = Q::from_integer(2).value;
    const auto quotient = Q::div(a_val, b_val);
    CHECK(quotient == Q::from_integer(3).value);

    // Dividing by same value gives 1
    CHECK(Q::div(Q::scale * 7, Q::scale * 7) == Q::scale);

    // Half division
    const auto one = Q::scale;
    const auto two = Q::from_integer(2).value;
    const auto half = Q::div(one, two);
    CHECK(half == Q::from_real(0.5).value);
}

TEST_CASE("fixed::Q mul is exact where a narrow product would overflow", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<10, std::int64_t>;

    // Squaring an Earth-scale coordinate reaches ~4.3e19 before the rescale, against an int64 max of 9.2e18.
    const std::int64_t earth_scale = Q::from_integer(6400000).value;
    const std::int64_t expected = Q::from_integer(6400000LL * 6400000LL).value;

    CHECK(Q::mul(earth_scale, earth_scale) == expected);
    CHECK(Q::mul(-earth_scale, earth_scale) == -expected);
    CHECK(Q::mul(-earth_scale, -earth_scale) == expected);
}

TEST_CASE("fixed::Q div is exact where a narrow numerator would overflow", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<10, std::int64_t>;

    // Scaling this numerator by 1024 reaches ~1.0e19, past the int64 max.
    const std::int64_t numerator = 10000000000000000LL;
    const std::int64_t two = Q::from_integer(2).value;

    CHECK(Q::div(numerator, two) == numerator / 2);
    CHECK(Q::div(-numerator, two) == -(numerator / 2));
}

TEST_CASE("fixed::Q from_real rounds in the requested direction", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<10, std::int64_t>;

    // 1.5 grid steps, so the three modes disagree.
    const double between = 1.5 / static_cast<double>(Q::scale);
    CHECK(Q::from_real(between).value == 2);
    CHECK(Q::from_real<std::round_to_nearest>(between).value == 2);
    CHECK(Q::from_real<std::round_toward_neg_infinity>(between).value == 1);
    CHECK(Q::from_real<std::round_toward_infinity>(between).value == 2);

    // Negative values round toward the named infinity, not toward zero.
    CHECK(Q::from_real<std::round_toward_neg_infinity>(-between).value == -2);
    CHECK(Q::from_real<std::round_toward_infinity>(-between).value == -1);

    // A value already on the grid is fixed under every mode.
    const double on_grid = 4.0 / static_cast<double>(Q::scale);
    CHECK(Q::from_real<std::round_to_nearest>(on_grid).value == 4);
    CHECK(Q::from_real<std::round_toward_neg_infinity>(on_grid).value == 4);
    CHECK(Q::from_real<std::round_toward_infinity>(on_grid).value == 4);
}

TEST_CASE("fp::orient detects triangle winding and collinearity", "[dag_builder][fixed_point]") {
    using Q = fixed::Q<16>;
    using FixedPoint = fp::Vec2<Q>;
    using FixedScalar = fp::Scalar<Q>;

    // CCW triple gives positive orient
    const auto a = FixedPoint::from_real(0.0, 0.0);
    const auto b = FixedPoint::from_real(1.0, 0.0);
    const auto p_ccw = FixedPoint::from_real(0.5, 0.5);
    const auto orient_ccw = fp::orient(a, b, p_ccw);
    CHECK(orient_ccw > FixedScalar::zero());

    // CW triple gives negative orient
    const auto p_cw = FixedPoint::from_real(0.5, -0.5);
    const auto orient_cw = fp::orient(a, b, p_cw);
    CHECK(orient_cw < FixedScalar::zero());

    // Collinear points give zero orient
    const auto p_collinear = FixedPoint::from_real(0.5, 0.0);
    const auto orient_collinear = fp::orient(a, b, p_collinear);
    CHECK(orient_collinear == FixedScalar::zero());

    // Collinear with integer coordinates
    const auto p1 = FixedPoint::from_real(0.0, 0.0);
    const auto p2 = FixedPoint::from_real(2.0, 0.0);
    const auto p3 = FixedPoint::from_real(1.0, 0.0);
    CHECK(fp::orient(p1, p2, p3) == FixedScalar::zero());

    // Half-coordinate collinearity
    const auto q1 = FixedPoint::from_real(0.0, 0.0);
    const auto q2 = FixedPoint::from_real(1.0, 0.0);
    const auto q3 = FixedPoint::from_real(0.5, 0.0);
    CHECK(fp::orient(q1, q2, q3) == FixedScalar::zero());
}
