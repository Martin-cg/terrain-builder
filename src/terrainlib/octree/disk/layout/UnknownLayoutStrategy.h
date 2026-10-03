#pragma once

#include <string>

#include <fmt/format.h>

namespace octree::disk::layout {

// A layout strategy id that no strategy is registered under.
struct UnknownLayoutStrategy {
    std::string id;

    bool operator==(const UnknownLayoutStrategy &) const = default;
};

} // namespace octree::disk::layout

template <>
struct fmt::formatter<octree::disk::layout::UnknownLayoutStrategy> {
    constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context &context) {
        return context.begin();
    }

    fmt::format_context::iterator format(const octree::disk::layout::UnknownLayoutStrategy &error, fmt::format_context &context) const {
        return fmt::format_to(context.out(), "unknown layout strategy {}", error.id);
    }
};
