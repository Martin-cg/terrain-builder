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

    // Creates an error with `code` that originates at `location`.
    // A `message` becomes its first context.
    template <OneOf<Es...> E>
    explicit BasicError(E code, std::optional<std::string> message = std::nullopt, const std::source_location location = std::source_location::current())
        : _code(std::move(code)),
          _trace(location) {
        if (message.has_value()) {
            this->context(std::move(*message), location);
        }
    }

    // Creates an erased error from `message`.
    explicit BasicError(std::string message, const std::source_location location = std::source_location::current())
        requires(is_erased)
        : _code(std::move(message)),
          _trace(location) {}

    // Takes over an error whose codes are all among Es, or any typed error when this error is erased, keeping its frames.
    template <typename... Fs>
        requires(is_erased || includes<Fs...>)
    BasicError(BasicError<Fs...> other)
        : _code(other._code),
          _trace(std::move(other._trace)) {}

    // Adds a frame holding `message`.
    template <typename Self>
    Self &&context(this Self &&self, std::string message, const std::source_location location = std::source_location::current()) {
        self._trace.push(Frame{.message = std::move(message), .location = location});
        return std::forward<Self>(self);
    }

    // Adds a frame without a message.
    template <typename Self>
    Self &&propagate(this Self &&self, const std::source_location location = std::source_location::current()) {
        self._trace.push(Frame{.location = location});
        return std::forward<Self>(self);
    }

    [[nodiscard]]
    const auto &code() const requires(sizeof...(Es) == 1) {
        return this->_code.get();
    }

    // The held code if it is an E, otherwise null.
    template <OneOf<Es...> E>
    [[nodiscard]]
    const E *get_if() const {
        return this->_code.template get_if<E>();
    }

    // Calls `f` with the held code.
    template <typename F>
        requires(!is_erased)
    decltype(auto) visit(F &&f) const {
        return this->_code.visit(std::forward<F>(f));
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
