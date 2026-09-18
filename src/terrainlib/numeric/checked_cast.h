#pragma once

#include <concepts>
#include <utility>

#include <glm/glm.hpp>
#include <libassert/assert.hpp>

#include "glm_utils.h"

// Converts `value` to `To`.
// Asserts that `value` is representable in `To`.
template <std::integral To, std::integral From>
[[nodiscard]] inline constexpr To checked_cast(const From value) {
    ASSERT(std::in_range<To>(value));
    return static_cast<To>(value);
}

// Converts `value` to `To`.
// Asserts in debug builds that `value` is representable in `To`.
template <std::integral To, std::integral From>
[[nodiscard]] inline constexpr To debug_checked_cast(const From value) {
    DEBUG_ASSERT(std::in_range<To>(value));
    return static_cast<To>(value);
}

// Converts each component of `v` to `To`.
// Asserts that every component is representable in `To`.
template <std::integral To, glm::length_t L, std::integral From, glm::qualifier Q>
[[nodiscard]] inline constexpr glm::vec<L, To, Q> checked_cast(const glm::vec<L, From, Q> &v) {
    return transform(v, [](const From c) {
        return checked_cast<To>(c);
    });
}

// Converts each component of `v` to `To`.
// Asserts in debug builds that every component is representable in `To`.
template <std::integral To, glm::length_t L, std::integral From, glm::qualifier Q>
[[nodiscard]] inline constexpr glm::vec<L, To, Q> debug_checked_cast(const glm::vec<L, From, Q> &v) {
    return transform(v, [](const From c) {
        return debug_checked_cast<To>(c);
    });
}