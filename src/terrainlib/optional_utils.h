#pragma once

#include <optional>
#include <functional>
#include <type_traits>

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

template <typename T>
std::optional<T> flatten(const std::optional<std::optional<T>> option) {
    if (option.has_value()) {
        return option.value();
    } else {
        return std::nullopt;
    }
}
