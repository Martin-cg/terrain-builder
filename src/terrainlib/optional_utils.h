#pragma once

#include <optional>
#include <functional>
#include <type_traits>
#include <utility>

#include <tl/expected.hpp>

template <typename T>
std::optional<std::reference_wrapper<const T>> as_ref(const std::optional<T> &option) {
    if (option.has_value()) {
        return option.value();
    } else {
        return std::nullopt;
    }
}
template <typename T>
std::optional<std::reference_wrapper<T>> as_ref(std::optional<T> &option) {
    if (option.has_value()) {
        return option.value();
    } else {
        return std::nullopt;
    }
}

template <typename T, typename Func>
auto map(const std::optional<T> &option, Func &&f) -> std::optional<std::invoke_result_t<Func, T>> {
    using U = std::invoke_result_t<Func, T>;

    if (option.has_value()) {
        return std::optional<U>{std::forward<Func>(f)(option.value())};
    } else {
        return std::nullopt;
    }
}

template <typename T, typename Func>
T& get_or_insert_with(std::optional<T> &option, Func &&f) {
    if (!option.has_value()) {
        T value = std::forward<Func>(f)();
        option = value;
    }
    return option.value();
}

template <typename T, typename Func>
T& get_or_insert(std::optional<T> &option, T value) {
    return get_or_insert_with(option, [=]() { return std::move(value); });
}

template <typename T, typename Func>
auto ok_or_else(std::optional<T> option, Func &&f) -> tl::expected<T, std::invoke_result_t<Func>> {
    if (option.has_value()) {
        return std::move(option).value();
    } else {
        return tl::unexpected(std::forward<Func>(f)());
    }
}

template <typename T, typename E>
tl::expected<T, E> ok_or(std::optional<T> option, E error) {
    return ok_or_else(std::move(option), [=]() { return std::move(error); });
}

template <typename T>
std::optional<T> flatten(const std::optional<std::optional<T>> option) {
    if (option.has_value()) {
        return option.value();
    } else {
        return std::nullopt;
    }
}
