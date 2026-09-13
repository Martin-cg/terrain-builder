#pragma once

#include <concepts>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

#include <tl/expected.hpp>

#include "error/Error.h"
#include "error/ErrorCode.h"
#include "error/Result.h"

namespace alp {

// An unexpected holding a new error with `code`.
template <ErrorCode E>
[[nodiscard]]
tl::unexpected<Error<E>> make_unexpected(E code, const std::source_location location = std::source_location::current()) {
    return tl::make_unexpected(Error<E>(std::move(code), location));
}

// An unexpected holding a new error with `code`, with `message` as its first context.
template <ErrorCode E>
[[nodiscard]]
tl::unexpected<Error<E>> make_unexpected(E code, std::string message, const std::source_location location = std::source_location::current()) {
    return tl::make_unexpected(Error<E>(std::move(code), std::move(message), location));
}

// An unexpected holding a new erased error with `message`.
[[nodiscard]]
inline tl::unexpected<Error<>> make_unexpected(std::string message, const std::source_location location = std::source_location::current()) {
    return tl::make_unexpected(Error<>(std::move(message), location));
}

// An unexpected holding `error`, with a frame added.
template <ErrorCode... Es>
[[nodiscard]]
tl::unexpected<Error<Es...>> make_unexpected(Error<Es...> error, const std::source_location location = std::source_location::current()) {
    error.propagate(location);
    return tl::make_unexpected(std::move(error));
}

// The value held by `value`, or an error with `code` if it is empty.
template <typename T, ErrorCode E>
[[nodiscard]]
Result<T, E> to_expected(std::optional<T> value, E code, const std::source_location location = std::source_location::current()) {
    if (value.has_value()) {
        return std::move(*value);
    }
    return alp::make_unexpected(std::move(code), location);
}

// The value held by `value`, or an error with `code` and `message` as its first context if it is empty.
template <typename T, ErrorCode E>
[[nodiscard]]
Result<T, E> to_expected(std::optional<T> value, E code, std::string message, const std::source_location location = std::source_location::current()) {
    if (value.has_value()) {
        return std::move(*value);
    }
    return alp::make_unexpected(std::move(code), std::move(message), location);
}

// `result`, with a frame holding `message` added if it failed.
template <typename T, ErrorCode... Es>
[[nodiscard]]
Result<T, Es...> context(Result<T, Es...> &&result, std::string message, const std::source_location location = std::source_location::current()) {
    if (!result.has_value()) {
        result.error().context(std::move(message), location);
    }
    return std::move(result);
}

// `result`, with a frame holding what `make_message` returns added if it failed; `make_message` runs only then.
template <typename T, ErrorCode... Es, std::invocable F>
    requires std::convertible_to<std::invoke_result_t<F>, std::string>
[[nodiscard]]
Result<T, Es...> with_context(Result<T, Es...> &&result, F &&make_message, const std::source_location location = std::source_location::current()) {
    if (!result.has_value()) {
        result.error().context(std::invoke(std::forward<F>(make_message)), location);
    }
    return std::move(result);
}

} // namespace alp
