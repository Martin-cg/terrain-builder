#pragma once

#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <type_traits>
#include <utility>

#include <fmt/format.h>
#include <tl/expected.hpp>

#include "error/CodeStorage.h"
#include "error/ErrorCode.h"
#include "error/Trace.h"
#include "type_utils.h"

namespace alp {

// An error with the codes Es, where a set among Es stands for its codes; each code is kept once, in the order it first appears.
template <ErrorCodes... Es>
using Error = unique_t<flatten_t<BasicError<Es...>>>;

// An error with a code of type Es, or an erased error when Es is empty, together with the frames it collected.
template <typename... Es>
class BasicError {
    static_assert((ErrorCode<Es> && ...), "an error holds only error codes; nest a set through alp::Error");

public:
    // Whether the error is erased, holding no code of its own.
    static constexpr bool is_erased = sizeof...(Es) == 0;

    // Whether Es includes every one of the codes Fs, of which there is at least one.
    template <ErrorCodes... Fs>
    static constexpr bool includes = !Error<Fs...>::is_erased && is_subset_v<Error<Fs...>, BasicError>;

    // Starts an error with `code` at `location`.
    template <OneOf<Es...> E>
    explicit BasicError(E code, const std::source_location location = std::source_location::current())
        : _code(std::move(code)),
          _trace(location) {}

    // Starts an error with `code` at `location`, with `message` as its first context.
    template <OneOf<Es...> E>
    explicit BasicError(E code, std::string message, const std::source_location location = std::source_location::current())
        : BasicError(std::move(code), location) {
        this->context(std::move(message), location);
    }

    // Starts an erased error from a message alone.
    explicit BasicError(std::string message, const std::source_location location = std::source_location::current())
        requires(is_erased)
        : _code(std::move(message)),
          _trace(location) {}

    // Takes over a typed error, keeping only its code's message.
    template <typename... Fs>
    BasicError(BasicError<Fs...> &&other)
        requires(is_erased)
        : _code(other._code),
          _trace(std::move(other._trace)) {}

    // Takes over an error whose codes are all among Es, keeping its frames.
    template <typename... Fs>
        requires(includes<Fs...>)
    BasicError(BasicError<Fs...> &&other)
        : _code(detail::widen_code<Es...>(other._code)),
          _trace(std::move(other._trace)) {}

    template <typename... Fs>
        requires(includes<Fs...>)
    BasicError(const BasicError<Fs...> &other)
        : _code(detail::widen_code<Es...>(other._code)),
          _trace(other._trace) {}

    // Adds a frame holding `message`.
    BasicError &context(std::string message, const std::source_location location = std::source_location::current()) & {
        this->_trace.push(Frame{.message = std::move(message), .location = location});
        return *this;
    }

    BasicError &&context(std::string message, const std::source_location location = std::source_location::current()) && {
        this->_trace.push(Frame{.message = std::move(message), .location = location});
        return std::move(*this);
    }

    // Adds a frame without a message.
    BasicError &propagate(const std::source_location location = std::source_location::current()) & {
        this->_trace.push(Frame{.location = location});
        return *this;
    }

    BasicError &&propagate(const std::source_location location = std::source_location::current()) && {
        this->_trace.push(Frame{.location = location});
        return std::move(*this);
    }

    [[nodiscard]]
    const auto &code() const
        requires(sizeof...(Es) == 1)
    {
        return this->_code.get();
    }

    // The held code if it is an E, otherwise null.
    template <OneOf<Es...> E>
    [[nodiscard]]
    const E *get_if() const {
        return this->_code.template get_if<E>();
    }

    // Whether this error holds `code`.
    template <OneOf<Es...> E>
    [[nodiscard]]
    bool holds(const E &code) const {
        return this->_code == CodeStorage<Es...>(code);
    }

    // The frames, innermost first.
    [[nodiscard]]
    std::span<const Frame> frames() const {
        return this->_trace.frames();
    }

    // This error with only the codes Fs if it holds one of them, otherwise this error unchanged; a set among Fs stands for its codes.
    template <ErrorCodes... Fs, typename Self>
        requires(includes<Fs...>)
    [[nodiscard]]
    tl::expected<Error<Fs...>, BasicError> narrow(this Self &&self) {
        return Error<Fs...>::narrow_from(std::forward<Self>(self));
    }

private:
    template <typename... Fs>
    friend class BasicError;

    BasicError(CodeStorage<Es...> code, Trace trace) : _code(std::move(code)), _trace(std::move(trace)) {}

    // Narrows `error` to the codes Es if it holds one of them, otherwise hands it back unchanged.
    template <typename Source>
    static tl::expected<BasicError, std::remove_cvref_t<Source>> narrow_from(Source &&error) {
        std::optional<CodeStorage<Es...>> code_result = detail::narrow_code<Es...>(error._code);
        if (!code_result.has_value()) {
            return tl::make_unexpected(std::forward<Source>(error));
        }
        return BasicError(std::move(*code_result), std::forward<Source>(error)._trace);
    }

    friend struct fmt::formatter<BasicError>;

    CodeStorage<Es...> _code;
    Trace _trace;
};

} // namespace alp
