#pragma once

#include <concepts>
#include <optional>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

#include <tl/expected.hpp>

#include "error/Error.h"
#include "error/ErrorCode.h"
#include "macros.h"
#include "type_utils.h"

namespace alp::detail {

// A plain code ALP_TRY caught from a failed result, with where it was caught and the message to add; it becomes the error the enclosing function returns.
template <ErrorCode E>
struct PendingCode {
    E code;
    std::optional<std::string> message;
    std::source_location location;

    // Becomes any other type the code converts to, dropping the location and the message.
    template <typename Target>
        requires(!is_specialization_of_v<Target, BasicError> && std::is_convertible_v<const E &, Target>)
    operator Target() const {
        return this->code;
    }

    // Becomes an Error with its origin where the code was caught and the message as its first context.
    template <ErrorCode... Es>
        requires std::is_convertible_v<Error<E>, BasicError<Es...>>
    operator BasicError<Es...>() const {
        return Error<E>(this->code, this->message, this->location);
    }
};

// What ALP_TRY returns for a failed `result`: its Error with a frame at `location` holding `message` unless it is nullopt, a PendingCode carrying `message` for a plain code, any other error unchanged, or nullopt for an optional.
template <typename R, typename M>
auto make_residual(R &&result, M message, const std::source_location location) {
    constexpr bool has_message = !std::same_as<M, std::nullopt_t>;
    if constexpr (!requires { result.error(); }) {
        static_assert(!has_message, "ALP_TRY_CONTEXT needs an expected: an optional has no place for the message");
        return std::nullopt;
    } else {
        using Failure = std::remove_cvref_t<decltype(std::forward<R>(result).error())>;
        if constexpr (is_specialization_of_v<Failure, BasicError>) {
            Failure error = std::forward<R>(result).error();
            if constexpr (has_message) {
                error.context(std::move(message), location);
            } else {
                error.propagate(location);
            }
            return tl::make_unexpected(std::move(error));
        } else if constexpr (ErrorCode<Failure>) {
            return tl::make_unexpected(PendingCode<Failure>{.code = std::forward<R>(result).error(), .message = std::move(message), .location = location});
        } else {
            static_assert(!has_message, "ALP_TRY_CONTEXT needs an Error or a plain code: any other error has no place for the message");
            return tl::make_unexpected(std::forward<R>(result).error());
        }
    }
}

// The value held by a succeeded `result`, as a reference into it.
template <typename R>
decltype(auto) take_value(R &&result) {
    static_assert(!std::is_void_v<typename std::remove_cvref_t<R>::value_type>, "ALP_TRY with a target needs a result that holds a value");
    return *std::forward<R>(result);
}

} // namespace alp::detail

// ALP_TRY_DISCARD with its result variable and message spelled out; the message is nullopt for none.
#define ALP_TRY_IMPL_DISCARD(result, message, ...)                                                              \
    do {                                                                                                        \
        auto result = (__VA_ARGS__);                                                                            \
        if (!result) {                                                                                          \
            return ::alp::detail::make_residual(std::move(result), (message), std::source_location::current()); \
        }                                                                                                       \
    } while (false)

// ALP_TRY_BIND with its result variable and message spelled out; the message is nullopt for none.
#define ALP_TRY_IMPL_BIND(result, target, message, ...)                                                     \
    auto result = (__VA_ARGS__);                                                                            \
    if (!result) {                                                                                          \
        return ::alp::detail::make_residual(std::move(result), (message), std::source_location::current()); \
    }                                                                                                       \
    target = ::alp::detail::take_value(std::move(result))

// Returns from the enclosing function if the expression, an expected or an optional, failed, passing the failure on as make_residual does.
// The expression is all remaining arguments, so it may contain commas.
#define ALP_TRY_DISCARD(...) ALP_TRY_IMPL_DISCARD(ALP_UNIQUE_IDENT(_alp_try_result_), ::std::nullopt, __VA_ARGS__)

// Like ALP_TRY_DISCARD, but binds the value to `target`, a declaration or an lvalue.
// A reference target refers into the result, which lives until the end of the scope; under an `if` the macro needs braces.
#define ALP_TRY_BIND(target, ...) ALP_TRY_IMPL_BIND(ALP_UNIQUE_IDENT(_alp_try_result_), target, ::std::nullopt, __VA_ARGS__)

// Like ALP_TRY_DISCARD, but adds `message`, which is evaluated only on failure.
#define ALP_TRY_DISCARD_CONTEXT(message, ...) ALP_TRY_IMPL_DISCARD(ALP_UNIQUE_IDENT(_alp_try_result_), message, __VA_ARGS__)

// Like ALP_TRY_BIND, but adds `message`, which is evaluated only on failure.
#define ALP_TRY_BIND_CONTEXT(target, message, ...) ALP_TRY_IMPL_BIND(ALP_UNIQUE_IDENT(_alp_try_result_), target, message, __VA_ARGS__)

// Picks the macro for ALP_TRY by argument count.
#define ALP_TRY_SELECT(_1, _2, NAME, ...) NAME

// ALP_TRY_CONTEXT(expr, message).
#define ALP_TRY_CONTEXT_2(expr, message) ALP_TRY_DISCARD_CONTEXT(message, expr)

// ALP_TRY_CONTEXT(target, expr, message).
#define ALP_TRY_CONTEXT_3(target, expr, message) ALP_TRY_BIND_CONTEXT(target, message, expr)

// Picks the macro for ALP_TRY_CONTEXT by argument count; any other count fails to compile.
#define ALP_TRY_CONTEXT_SELECT(_1, _2, _3, NAME, ...) NAME

// ALP_TRY(expr) or ALP_TRY(target, expr); an expression with commas outside parentheses needs ALP_TRY_DISCARD or ALP_TRY_BIND.
#define ALP_TRY(...) ALP_TRY_SELECT(__VA_ARGS__, ALP_TRY_BIND, ALP_TRY_DISCARD)(__VA_ARGS__)

// ALP_TRY_CONTEXT(expr, message) or ALP_TRY_CONTEXT(target, expr, message).
#define ALP_TRY_CONTEXT(...) ALP_TRY_CONTEXT_SELECT(__VA_ARGS__, ALP_TRY_CONTEXT_3, ALP_TRY_CONTEXT_2, ALP_TRY_CONTEXT_needs_2_or_3_arguments)(__VA_ARGS__)
