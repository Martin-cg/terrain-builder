#pragma once

#include <ranges>
#include <span>
#include <string>

#include <fmt/format.h>

#include "error/Error.h"
#include "error/ErrorCode.h"
#include "error/Trace.h"

template <alp::ErrorCode... Es>
struct fmt::formatter<alp::Error<Es...>> {
    constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context &context) {
        auto position = context.begin();
        if (position != context.end() && *position == '#') {
            this->_is_multiline = true;
            ++position;
        }
        if (position != context.end() && *position != '}') {
            throw fmt::format_error("an error formats with {} or {:#}");
        }
        return position;
    }

    fmt::format_context::iterator format(const alp::Error<Es...> &error, fmt::format_context &context) const {
        const std::span<const alp::Frame> frames = error.frames();
        const std::string cause = error._code.message();
        fmt::format_context::iterator output = context.out();

        for (const alp::Frame &frame : frames | std::views::reverse) {
            if (frame.message.has_value()) {
                output = fmt::format_to(output, "{}: ", *frame.message);
            }
        }
        output = fmt::format_to(output, "{}", cause);

        if (!this->_is_multiline) {
            return output;
        }

        for (const alp::Frame &frame : frames | std::views::reverse) {
            output = fmt::format_to(output, "\n");

            const bool is_origin = &frame == &frames.front();
            if (is_origin) {
                output = fmt::format_to(output, "{} ", cause);
            } else if (frame.message.has_value()) {
                output = fmt::format_to(output, "{} ", *frame.message);
            }
            output = fmt::format_to(output, "at {}:{} in {}", frame.location.file_name(), frame.location.line(), frame.location.function_name());
        }
        return output;
    }

private:
    bool _is_multiline = false;
};
