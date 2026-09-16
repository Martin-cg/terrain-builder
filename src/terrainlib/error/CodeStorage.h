#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include "error/ErrorCode.h"
#include "type_utils.h"

namespace alp {

// The code of an error: a code of type Es, or for an erased error its message.
template <ErrorCode... Es>
class CodeStorage {
public:
    template <OneOf<Es...> E>
    explicit CodeStorage(E code) : _code(std::move(code)) {}

    // The code, for a storage of a single code.
    [[nodiscard]]
    const auto &get() const
        requires(sizeof...(Es) == 1)
    {
        return std::get<0>(this->_code);
    }

    // The held code if it is an E, otherwise null.
    template <OneOf<Es...> E>
    [[nodiscard]]
    const E *get_if() const {
        return std::get_if<E>(&this->_code);
    }

    [[nodiscard]]
    std::string message() const {
        return this->visit([](const auto &code) { return detail::format_code(code); });
    }

    // Calls `f` with the code it holds.
    template <typename F>
    decltype(auto) visit(F &&f) const {
        return std::visit(std::forward<F>(f), this->_code);
    }

    bool operator==(const CodeStorage &) const = default;

private:
    std::variant<Es...> _code;
};

template <>
class CodeStorage<> {
public:
    explicit CodeStorage(std::string message) : _message(std::move(message)) {}

    // Erases `storage`, keeping only its message.
    template <ErrorCode... Fs>
    explicit CodeStorage(const CodeStorage<Fs...> &storage) : _message(storage.message()) {}

    [[nodiscard]]
    std::string message() const {
        return this->_message;
    }

private:
    std::string _message;
};

namespace detail {

// `storage` as a storage of the codes Es, which include every code it can hold.
template <ErrorCode... Es, ErrorCode... Fs>
CodeStorage<Es...> widen_code(const CodeStorage<Fs...> &storage) {
    return storage.visit([](const auto &code) { return CodeStorage<Es...>(code); });
}

// The code `storage` holds, as a storage of the codes Fs, or nothing if it holds none of them.
template <ErrorCode... Fs, ErrorCode... Es>
std::optional<CodeStorage<Fs...>> narrow_code(const CodeStorage<Es...> &storage) {
    return storage.visit([](const auto &code) -> std::optional<CodeStorage<Fs...>> {
        if constexpr (OneOf<std::remove_cvref_t<decltype(code)>, Fs...>) {
            return CodeStorage<Fs...>(code);
        } else {
            return std::nullopt;
        }
    });
}

} // namespace detail

} // namespace alp