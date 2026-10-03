#pragma once

#include <filesystem>

#include <fmt/format.h>

namespace io {

// An extension that names no format the operation reads or writes.
struct UnsupportedFormat {
    std::filesystem::path extension;

    bool operator==(const UnsupportedFormat &) const = default;
};

} // namespace io

template <>
struct fmt::formatter<io::UnsupportedFormat> {
    constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context &context) {
        return context.begin();
    }

    fmt::format_context::iterator format(const io::UnsupportedFormat &error, fmt::format_context &context) const {
        return fmt::format_to(context.out(), "unsupported format {}", error.extension.string());
    }
};
