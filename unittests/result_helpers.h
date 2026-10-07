#pragma once

#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#include "error/format.h"
#include "macros.h"

// REQUIRE_VALUE(expr) with its result variable spelled out.
#define REQUIRE_VALUE_IMPL(result, ...)                                             \
    [&] {                                                                           \
        auto result = (__VA_ARGS__);                                                \
        if (!result.has_value()) {                                                  \
            FAIL(::fmt::format("the result holds an error\n{:?}", result.error())); \
        }                                                                           \
        return std::move(result).value();                                           \
    }()

// The value of `result`; fails the test with the error's report if it holds none.
#define REQUIRE_VALUE(...) REQUIRE_VALUE_IMPL(ALP_UNIQUE_IDENT(_require_value_result_), __VA_ARGS__)

// REQUIRE_ERROR(expr) with its result variable spelled out.
#define REQUIRE_ERROR_IMPL(result, ...)       \
    [&] {                                     \
        auto result = (__VA_ARGS__);          \
        if (result.has_value()) {             \
            FAIL("the result holds a value"); \
        }                                     \
        return std::move(result).error();     \
    }()

// The error of `result`; fails the test if it holds a value.
#define REQUIRE_ERROR(...) REQUIRE_ERROR_IMPL(ALP_UNIQUE_IDENT(_require_error_result_), __VA_ARGS__)
