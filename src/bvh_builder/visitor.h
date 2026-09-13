#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

#include "error/Result.h"
#include "error/try.h"

namespace visitor {

template <class F>
class Visitor {
public:
    Visitor(F visitor) : _visitor(visitor) {}

    template <class... Args>
    bool operator()(Args &&...args) {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, Args...>>;

        if constexpr (std::same_as<Result, void>) {
            std::invoke(_visitor, std::forward<Args>(args)...);
            return true;
        } else {
            static_assert(std::same_as<Result, bool>, "visitor must return void or bool");
            return std::invoke(_visitor, std::forward<Args>(args)...);
        }
    }

private:
    F _visitor;
};

template <class E, class F>
class FallibleVisitor {
public:
    FallibleVisitor(F visitor) : _visitor(visitor) {}

    template <class... Args>
    [[nodiscard]]
    alp::Result<bool, E> operator()(Args &&...args) {
        using Result = std::remove_cvref_t<std::invoke_result_t<F, Args...>>;

        if constexpr (std::same_as<Result, void>) {
            std::invoke(_visitor, std::forward<Args>(args)...);
            return true;
        } else if constexpr (std::same_as<Result, bool>) {
            return std::invoke(_visitor, std::forward<Args>(args)...);
        } else if constexpr (std::same_as<Result, alp::Result<void, E>>) {
            ALP_TRY(std::invoke(_visitor, std::forward<Args>(args)...));
            return true;
        } else {
            static_assert(std::same_as<Result, alp::Result<bool, E>>, "fallible visitor must return void, bool, alp::Result<void, E>, or alp::Result<bool, E>");
            return std::invoke(_visitor, std::forward<Args>(args)...);
        }
    }

private:
    F _visitor;
};

} // namespace visitor
