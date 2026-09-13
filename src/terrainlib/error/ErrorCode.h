#pragma once

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>

#include <fmt/format.h>

#include "type_utils.h"

namespace alp {

// An enum or a class other than a string, formattable with fmt; the formatted text is the code's message.
template <typename E>
concept ErrorCode = std::same_as<E, std::remove_cvref_t<E>>
    && (std::is_enum_v<E> || std::is_class_v<E>)
    && !is_specialization_of_v<E, std::basic_string>
    && !is_specialization_of_v<E, std::basic_string_view>
    && fmt::is_formattable<E>::value;

namespace detail {

template <ErrorCode E>
std::string format_code(const E &code) {
    return fmt::format("{}", code);
}

} // namespace detail

} // namespace alp
