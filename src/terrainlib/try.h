#pragma once

#include <optional>
#include <type_traits>
#include <utility>

#include <tl/expected.hpp>

#include "macros.h"

namespace try_detail {

// What is left of a result once its value is gone: an error, or nothing at all.
template <class Result>
auto residual(Result &&result) {
    if constexpr (requires { result.error(); }) {
        return tl::make_unexpected(std::forward<Result>(result).error());
    } else {
        return std::nullopt;
    }
}

} // namespace try_detail

#define TRY_IMPL_1(result, expr)                            \
    do {                                                    \
        auto result = (expr);                               \
        if (!result) {                                      \
            return try_detail::residual(std::move(result)); \
        }                                                   \
    } while (false)

#define TRY_IMPL_2(result, var, expr)                   \
    auto result = (expr);                               \
    if (!result) {                                      \
        return try_detail::residual(std::move(result)); \
    }                                                   \
    var = *std::move(result)

#define TRY_SELECT(_1, _2, NAME, ...) NAME

#define TRY_1(expr) TRY_IMPL_1(CAT_IDENT(_try_result_, __LINE__), expr)
#define TRY_2(var, expr) TRY_IMPL_2(CAT_IDENT(_try_result_, __LINE__), var, expr)

#define TRY(...) TRY_SELECT(__VA_ARGS__, TRY_2, TRY_1)(__VA_ARGS__)
