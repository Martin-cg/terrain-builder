#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "numeric/number_types.h"

namespace fixed {

namespace detail {

// `a * b / c` rounded to nearest, formed at the width a product of `FractionBits`-scaled values needs.
template <int FractionBits, typename T>
[[nodiscard]] constexpr T mul_div_round(const T a, const T b, const T c) noexcept {
    constexpr std::size_t product_bits = std::numeric_limits<T>::digits + FractionBits + 1;
    using wide = typename integer_for_width<storage_bits_v<product_bits>, std::is_signed_v<T>>::type;

    const wide wide_a = a;
    const wide wide_b = b;
    const wide wide_c = c;
    return static_cast<T>(int_div_round<wide>(wide_a * wide_b, wide_c));
}

} // namespace detail

template <int FractionBits, class Rep = std::int64_t>
struct Q {
    static_assert(std::is_signed_v<Rep>);
    static_assert(FractionBits > 0);

    static_assert(FractionBits < std::numeric_limits<Rep>::digits);

    using repr_t = Rep;

    static constexpr int fraction_bits = FractionBits;
    static constexpr repr_t scale = repr_t{1} << fraction_bits;

    // Raw fixed-point storage. One integer unit equals 1 / scale.
    repr_t value = 0;

    // Converts an integral coordinate into fixed-point storage.
    static constexpr Q from_integer(repr_t x) noexcept {
        return Q{x * scale};
    }

    // Converts a floating-point coordinate to the nearest fixed-point value.
    static Q from_real(double x) {
        return Q{static_cast<repr_t>(std::llround(x * double(scale)))};
    }

    // Converts fixed-point storage back to floating-point form.
    constexpr double real() const noexcept {
        return double(value) / double(scale);
    }

    // Multiplies two fixed-point values and rescales the result.
    static constexpr repr_t mul(repr_t a, repr_t b) noexcept {
        return detail::mul_div_round<FractionBits>(a, b, scale);
    }

    // Divides two fixed-point values while preserving fixed-point scale.
    static constexpr repr_t div(repr_t a, repr_t b) noexcept {
        return detail::mul_div_round<FractionBits>(a, scale, b);
    }
};

} // namespace fixed
