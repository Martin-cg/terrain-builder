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

// Creates an unexpected holding a new error with `code` that originates at the caller.
// A `message` becomes the error's first context.
template <ErrorCode E>
[[nodiscard]]
tl::unexpected<Error<E>> make_unexpected(E code, std::optional<std::string> message = std::nullopt, const std::source_location location = std::source_location::current()) {
    return tl::make_unexpected(Error<E>(std::move(code), std::move(message), location));
}

// Creates an unexpected holding a new erased error with `message` that originates at the caller.
[[nodiscard]]
inline tl::unexpected<Error<>> make_unexpected(std::string message, const std::source_location location = std::source_location::current()) {
    return tl::make_unexpected(Error<>(std::move(message), location));
}

// Adds a frame at the caller to `error` and wraps it in an unexpected.
template <ErrorCode... Es>
[[nodiscard]]
tl::unexpected<BasicError<Es...>> make_unexpected(BasicError<Es...> error, const std::source_location location = std::source_location::current()) {
    error.propagate(location);
    return tl::make_unexpected(std::move(error));
}

// Turns `value` into a result that fails with `code` when it is empty.
// A `message` becomes the error's first context.
template <typename T, ErrorCode E>
[[nodiscard]]
Result<T, E> to_expected(std::optional<T> value, E code, std::optional<std::string> message = std::nullopt, const std::source_location location = std::source_location::current()) {
    if (value.has_value()) {
        return std::move(*value);
    }
    return alp::make_unexpected(std::move(code), std::move(message), location);
}

// Adds a frame holding `message` to `result` if it failed.
template <typename T, ErrorCode... Es>
[[nodiscard]]
BasicResult<T, BasicError<Es...>> context(BasicResult<T, BasicError<Es...>> &&result, std::string message, const std::source_location location = std::source_location::current()) {
    if (!result.has_value()) {
        result.error().context(std::move(message), location);
    }
    return std::move(result);
}

// Adds a frame holding the result of `make_message` to `result` if it failed.
// `make_message` only runs on failure.
template <typename T, ErrorCode... Es, std::invocable F>
    requires std::convertible_to<std::invoke_result_t<F>, std::string>
[[nodiscard]]
BasicResult<T, BasicError<Es...>> with_context(BasicResult<T, BasicError<Es...>> &&result, F &&make_message, const std::source_location location = std::source_location::current()) {
    if (!result.has_value()) {
        result.error().context(std::invoke(std::forward<F>(make_message)), location);
    }
    return std::move(result);
}

} // namespace alp
