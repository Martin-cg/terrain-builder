#pragma once

#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <utility>

#include <fmt/format.h>
#include <tl/expected.hpp>

#include "error/CodeStorage.h"
#include "error/ErrorCode.h"
#include "error/Trace.h"
#include "type_utils.h"

namespace alp {

// An error with a code of type Es, or an erased error when Es is empty, together with the frames it collected.
template <ErrorCode... Es>
class Error {
public:
    // Whether the error is erased, holding no code of its own.
    static constexpr bool is_erased = sizeof...(Es) == 0;

    // Whether Es includes every one of the codes Fs, of which there is at least one.
    template <ErrorCode... Fs>
    static constexpr bool includes = sizeof...(Fs) > 0 && (OneOf<Fs, Es...> && ...);

    // Starts an error with `code` at `location`.
    template <OneOf<Es...> E>
    explicit Error(E code, const std::source_location location = std::source_location::current())
        : _code(std::move(code)),
          _trace(location) {}

    // Starts an error with `code` at `location`, with `message` as its first context.
    template <OneOf<Es...> E>
    explicit Error(E code, std::string message, const std::source_location location = std::source_location::current())
        : Error(std::move(code), location) {
        this->context(std::move(message), location);
    }

    // Starts an erased error from a message alone.
    explicit Error(std::string message, const std::source_location location = std::source_location::current())
        requires(is_erased)
        : _code(std::move(message)),
          _trace(location) {}

    // Takes over a typed error, keeping only its code's message.
    template <ErrorCode... Fs>
    Error(Error<Fs...> &&other)
        requires(is_erased)
        : _code(other._code),
          _trace(std::move(other._trace)) {}

    // Takes over an error whose codes are all among Es, keeping its frames.
    template <ErrorCode... Fs>
        requires(includes<Fs...>)
    Error(Error<Fs...> &&other)
        : _code(detail::widen_code<Es...>(other._code)),
          _trace(std::move(other._trace)) {}

    template <ErrorCode... Fs>
        requires(includes<Fs...>)
    Error(const Error<Fs...> &other)
        : _code(detail::widen_code<Es...>(other._code)),
          _trace(other._trace) {}

    // Adds a frame holding `message`.
    Error &context(std::string message, const std::source_location location = std::source_location::current()) & {
        this->_trace.push(Frame{.message = std::move(message), .location = location});
        return *this;
    }

    Error &&context(std::string message, const std::source_location location = std::source_location::current()) && {
        this->_trace.push(Frame{.message = std::move(message), .location = location});
        return std::move(*this);
    }

    // Adds a frame without a message.
    Error &propagate(const std::source_location location = std::source_location::current()) & {
        this->_trace.push(Frame{.location = location});
        return *this;
    }

    Error &&propagate(const std::source_location location = std::source_location::current()) && {
        this->_trace.push(Frame{.location = location});
        return std::move(*this);
    }

    [[nodiscard]]
    const auto &code() const
        requires(sizeof...(Es) == 1)
    {
        return this->_code.get();
    }

    // The frames, innermost first.
    [[nodiscard]]
    std::span<const Frame> frames() const {
        return this->_trace.frames();
    }

    // This error with only the codes Fs if it holds one of them, otherwise this error unchanged.
    template <ErrorCode... Fs>
        requires(includes<Fs...>)
    [[nodiscard]]
    tl::expected<Error<Fs...>, Error> narrow() && {
        std::optional<CodeStorage<Fs...>> code_result = detail::narrow_code<Fs...>(this->_code);
        if (!code_result.has_value()) {
            return tl::make_unexpected(std::move(*this));
        }
        return Error<Fs...>(std::move(*code_result), std::move(this->_trace));
    }

    template <ErrorCode... Fs>
        requires(includes<Fs...>)
    [[nodiscard]]
    tl::expected<Error<Fs...>, Error> narrow() const & {
        Error copy = *this;
        return std::move(copy).template narrow<Fs...>();
    }

private:
    template <ErrorCode... Fs>
    friend class Error;

    Error(CodeStorage<Es...> code, Trace trace) : _code(std::move(code)), _trace(std::move(trace)) {}

    friend struct fmt::formatter<Error>;

    CodeStorage<Es...> _code;
    Trace _trace;
};

} // namespace alp
