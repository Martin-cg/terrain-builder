#pragma once

#include <cstdint>
#include <ranges>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "enumerate.h"
#include "error/Error.h"
#include "error/ErrorCode.h"
#include "error/Trace.h"

// Formats an error as its outermost message with {}, as all of its messages on one line with {:#}, or as a report of its causes and its trace with {:?}.
template <alp::ErrorCode... Es>
struct fmt::formatter<alp::BasicError<Es...>> {
    constexpr fmt::format_parse_context::iterator parse(fmt::format_parse_context &context) {
        auto position = context.begin();
        if (position != context.end() && *position == '#') {
            this->_form = Form::Chain;
            ++position;
        } else if (position != context.end() && *position == '?') {
            this->_form = Form::Report;
            ++position;
        }
        if (position != context.end() && *position != '}') {
            throw fmt::format_error("an error formats with {}, {:#} or {:?}");
        }
        return position;
    }

    fmt::format_context::iterator format(const alp::BasicError<Es...> &error, fmt::format_context &context) const {
        const std::span<const alp::Frame> frames = error.frames();
        const std::string cause = error._code.message();

        std::vector<std::string_view> messages;
        for (const alp::Frame &frame : frames | std::views::reverse) {
            if (frame.message.has_value()) {
                messages.push_back(*frame.message);
            }
        }
        messages.push_back(cause);

        fmt::format_context::iterator output = fmt::format_to(context.out(), "{}", messages.front());
        if (this->_form == Form::Message) {
            return output;
        }
        if (this->_form == Form::Chain) {
            for (const std::string_view message : messages | std::views::drop(1)) {
                output = fmt::format_to(output, ": {}", message);
            }
            return output;
        }

        const std::span<const std::string_view> causes = std::span(messages).subspan(1);
        if (!causes.empty()) {
            // Numbering a single cause reads as a list that lost its other entries, so it stays plain.
            const bool is_numbered = causes.size() > 1;
            output = fmt::format_to(output, "\n\nCaused by:");
            for (const auto [index, message] : enumerate<uint32_t>(causes)) {
                if (is_numbered) {
                    output = fmt::format_to(output, "\n{:>5}: {}", index, message);
                } else {
                    output = fmt::format_to(output, "\n    {}", message);
                }
            }
        }

        output = fmt::format_to(output, "\n\nTrace:");
        for (const alp::Frame &frame : frames | std::views::reverse) {
            output = write_location(output, frame.location);
        }
        return write_location(output, error.origin());
    }

private:
    // How an error was asked to format.
    enum class Form {
        // Its outermost message.
        Message,

        // All of its messages on one line, outermost first.
        Chain,

        // Its outermost message, the causes below it, and the trace.
        Report,
    };

    static fmt::format_context::iterator write_location(fmt::format_context::iterator output, const std::source_location location) {
        return fmt::format_to(output, "\n    at {}:{} in {}", location.file_name(), location.line(), location.function_name());
    }

    Form _form = Form::Message;
};