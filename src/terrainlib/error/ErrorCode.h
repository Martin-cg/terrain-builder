#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

#include <fmt/format.h>

#include "type_utils.h"

namespace alp {

template <typename... Es>
class BasicError;

// An enum or a class other than a string or an error, formattable with fmt; the formatted text is the code's message.
template <typename E>
concept ErrorCode = std::same_as<E, std::remove_cvref_t<E>>
    && (std::is_enum_v<E> || std::is_class_v<E>)
    && !is_specialization_of_v<E, std::basic_string>
    && !is_specialization_of_v<E, std::basic_string_view>
    && !is_specialization_of_v<E, BasicError>
    && fmt::is_formattable<E>::value;

// A code, or a set of codes that is not erased.
template <typename T>
concept ErrorCodes = ErrorCode<T> || (is_specialization_of_v<T, BasicError> && !T::is_erased);

namespace detail {

template <ErrorCode E>
std::string format_code(const E &code) {
    return fmt::format("{}", code);
}

} // namespace detail

} // namespace alp
