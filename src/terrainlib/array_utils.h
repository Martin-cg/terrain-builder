#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <span>
#include <type_traits>

#include <libassert/assert.hpp>

// The `N` elements of `input`, which has to hold exactly that many.
template <size_t N, typename T>
std::array<T, N> to_array(const std::span<const T> input) {
    ASSERT(input.size() == N, "the span does not hold the expected number of elements");

    std::array<T, N> result{};
    std::ranges::copy(input, result.begin());
    return result;
}

template <typename T, size_t N, typename Func>
auto transform_array(const std::array<T, N> &input, Func transform) {
    using Output = std::invoke_result_t<Func, const T &>;
    std::array<Output, N> result{};
    for (size_t i = 0; i < N; i++) {
        result[i] = std::invoke(transform, input[i]);
    }
    return result;
}

template <typename T, size_t N, typename Func>
auto transform_array(std::span<const T, N> input, Func &&transform) {
    static_assert(N != std::dynamic_extent);
    using Output = std::remove_cvref_t<std::invoke_result_t<Func &, const T &>>;

    std::array<Output, N> result{};
    for (size_t i = 0; i < N; ++i) {
        result[i] = std::invoke(transform, input[i]);
    }
    return result;
}
