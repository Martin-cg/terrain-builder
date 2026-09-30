#pragma once

#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>

#include "error/format.h"

// The value of `result`; fails the test with the error's report if it holds none.
#define REQUIRE_VALUE(...)                                                          \
    [&] {                                                                           \
        auto result = (__VA_ARGS__);                                                \
        if (!result.has_value()) {                                                  \
            FAIL(::fmt::format("the result holds an error\n{:?}", result.error())); \
        }                                                                           \
        return std::move(result).value();                                           \
    }()

// The error of `result`; fails the test if it holds a value.
#define REQUIRE_ERROR(...)                    \
    [&] {                                     \
        auto result = (__VA_ARGS__);          \
        if (result.has_value()) {             \
            FAIL("the result holds a value"); \
        }                                     \
        return std::move(result).error();     \
    }()
