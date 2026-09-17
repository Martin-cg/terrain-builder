#pragma once

#include <source_location>
#include <string_view>

// Whether `a` and `b` name the same file, function, line and column.
inline bool is_same_location(const std::source_location &a, const std::source_location &b) {
    return std::string_view(a.file_name()) == b.file_name()
        && std::string_view(a.function_name()) == b.function_name()
        && a.line() == b.line()
        && a.column() == b.column();
}