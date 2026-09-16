#pragma once

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
        Error<E> error(this->code, this->location);
        if (this->message.has_value()) {
            error.context(*this->message, this->location);
        }
        return error;
    }
};

// What ALP_TRY returns for a failed `result`: its Error with a frame at `location`, a PendingCode for a plain code, any other error unchanged, or nullopt.
template <typename R>
auto make_residual(R &&result, const std::source_location location) {
    if constexpr (!requires { result.error(); }) {
        return std::nullopt;
    } else {
        using Failure = std::remove_cvref_t<decltype(std::forward<R>(result).error())>;
        if constexpr (is_specialization_of_v<Failure, BasicError>) {
            Failure error = std::forward<R>(result).error();
            error.propagate(location);
            return tl::make_unexpected(std::move(error));
        } else if constexpr (ErrorCode<Failure>) {
            return tl::make_unexpected(PendingCode<Failure>{.code = std::forward<R>(result).error(), .message = std::nullopt, .location = location});
        } else {
            return tl::make_unexpected(std::forward<R>(result).error());
        }
    }
}

// What ALP_TRY_CONTEXT returns for a failed `result`: its Error with a frame holding `message`, or a PendingCode carrying `message`.
template <typename R>
auto make_residual_with_context(R &&result, std::string message, const std::source_location location) {
    if constexpr (!requires { result.error(); }) {
        static_assert(always_false_v<R>, "ALP_TRY_CONTEXT needs an expected: an optional has no place for the message");
    } else {
        using Failure = std::remove_cvref_t<decltype(std::forward<R>(result).error())>;
        if constexpr (is_specialization_of_v<Failure, BasicError>) {
            Failure error = std::forward<R>(result).error();
            error.context(std::move(message), location);
            return tl::make_unexpected(std::move(error));
        } else if constexpr (ErrorCode<Failure>) {
            return tl::make_unexpected(PendingCode<Failure>{.code = std::forward<R>(result).error(), .message = std::move(message), .location = location});
        } else {
            static_assert(always_false_v<R>, "ALP_TRY_CONTEXT needs an Error or a plain code: any other error has no place for the message");
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

// Evaluates the expression into the variable `result` inside a scope of its own, and returns what make_residual makes of it if it failed.
#define ALP_TRY_IMPL_DISCARD(result, ...)                                                            \
    do {                                                                                             \
        auto result = (__VA_ARGS__);                                                                 \
        if (!result) {                                                                               \
            return ::alp::detail::make_residual(std::move(result), std::source_location::current()); \
        }                                                                                            \
    } while (false)

// Evaluates the expression into the variable `result` in the enclosing scope, returns what make_residual makes of it if it failed, and otherwise binds its value to `target`.
#define ALP_TRY_IMPL_BIND(result, target, ...)                                                   \
    auto result = (__VA_ARGS__);                                                                 \
    if (!result) {                                                                               \
        return ::alp::detail::make_residual(std::move(result), std::source_location::current()); \
    }                                                                                            \
    target = ::alp::detail::take_value(std::move(result))

// Like ALP_TRY_IMPL_DISCARD, but returns what make_residual_with_context makes of the failure; `message` is evaluated only inside the failure branch.
#define ALP_TRY_IMPL_DISCARD_CONTEXT(result, message, ...)                                                                    \
    do {                                                                                                                      \
        auto result = (__VA_ARGS__);                                                                                          \
        if (!result) {                                                                                                        \
            return ::alp::detail::make_residual_with_context(std::move(result), (message), std::source_location::current()); \
        }                                                                                                                     \
    } while (false)

// Like ALP_TRY_IMPL_BIND, but returns what make_residual_with_context makes of the failure; `message` is evaluated only inside the failure branch.
#define ALP_TRY_IMPL_BIND_CONTEXT(result, target, message, ...)                                                           \
    auto result = (__VA_ARGS__);                                                                                          \
    if (!result) {                                                                                                        \
        return ::alp::detail::make_residual_with_context(std::move(result), (message), std::source_location::current()); \
    }                                                                                                                     \
    target = ::alp::detail::take_value(std::move(result))

// Evaluates the expression, an expected or an optional, and returns from the enclosing function if it failed; a succeeded value is dropped.
// On failure the enclosing function returns, depending on the error:
// - an Error: the same error, with a frame at this line added;
// - a plain code: the code itself, or an Error with its origin at this line when the enclosing function returns an Error;
// - any other error: the error unchanged;
// - an empty optional: an empty optional.
// The expression is the whole remaining argument list, so commas in it, such as in braced options, need no extra parentheses.
// Code written inside the arguments records the line of this macro rather than its own.
#define ALP_TRY_DISCARD(...) ALP_TRY_IMPL_DISCARD(ALP_UNIQUE_IDENT(_alp_try_result_), __VA_ARGS__)

// Like ALP_TRY_DISCARD, but binds the value of a succeeded expression to `target`, a declaration such as `const Batch &batch` or an assignable lvalue such as `this->_rows`.
// A reference target refers into the result, which lives until the end of the enclosing scope; the macro expands to several statements, so under an `if` it needs braces.
#define ALP_TRY_BIND(target, ...) ALP_TRY_IMPL_BIND(ALP_UNIQUE_IDENT(_alp_try_result_), target, __VA_ARGS__)

// Like ALP_TRY_DISCARD, and on failure adds `message` as context; `message` is evaluated only on failure.
// To an Error the message is a frame at this line, to a plain code the first context of the Error made from it; an optional or any other error has no place for it.
#define ALP_TRY_DISCARD_CONTEXT(message, ...) ALP_TRY_IMPL_DISCARD_CONTEXT(ALP_UNIQUE_IDENT(_alp_try_result_), message, __VA_ARGS__)

// Like ALP_TRY_BIND, and on failure adds `message` as context; `message` is evaluated only on failure.
#define ALP_TRY_BIND_CONTEXT(target, message, ...) ALP_TRY_IMPL_BIND_CONTEXT(ALP_UNIQUE_IDENT(_alp_try_result_), target, message, __VA_ARGS__)

// Picks ALP_TRY_BIND or ALP_TRY_DISCARD by how many arguments ALP_TRY was given.
#define ALP_TRY_SELECT(_1, _2, NAME, ...) NAME

// ALP_TRY_CONTEXT with two arguments, in the order ALP_TRY_DISCARD_CONTEXT takes them.
#define ALP_TRY_CONTEXT_2(expr, message) ALP_TRY_DISCARD_CONTEXT(message, expr)

// ALP_TRY_CONTEXT with three arguments, in the order ALP_TRY_BIND_CONTEXT takes them.
#define ALP_TRY_CONTEXT_3(target, expr, message) ALP_TRY_BIND_CONTEXT(target, message, expr)

// Picks ALP_TRY_CONTEXT_3 or ALP_TRY_CONTEXT_2 by how many arguments ALP_TRY_CONTEXT was given; with one argument it picks a name that does not exist, so the call fails to compile.
#define ALP_TRY_CONTEXT_SELECT(_1, _2, _3, NAME, ...) NAME

// ALP_TRY(expr) is ALP_TRY_DISCARD(expr) and ALP_TRY(target, expr) is ALP_TRY_BIND(target, expr), picked by the number of arguments; an expression with commas outside parentheses needs the dedicated form.
#define ALP_TRY(...) ALP_TRY_SELECT(__VA_ARGS__, ALP_TRY_BIND, ALP_TRY_DISCARD)(__VA_ARGS__)

// ALP_TRY_CONTEXT(expr, message) is ALP_TRY_DISCARD_CONTEXT(message, expr) and ALP_TRY_CONTEXT(target, expr, message) is ALP_TRY_BIND_CONTEXT(target, message, expr), picked by the number of arguments.
#define ALP_TRY_CONTEXT(...) ALP_TRY_CONTEXT_SELECT(__VA_ARGS__, ALP_TRY_CONTEXT_3, ALP_TRY_CONTEXT_2, ALP_TRY_CONTEXT_needs_2_or_3_arguments)(__VA_ARGS__)
