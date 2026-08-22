#pragma once

#include <cstddef>

template <typename T>
constexpr size_t sizeof_val(const T &value) {
    if constexpr (requires { value.size_bytes(); }) {
        return value.size_bytes();
    } else {
        return sizeof(value);
    }
}

template <typename... T>
constexpr size_t sizeof_val(const T &...values) {
    return (sizeof_val(values) + ... + size_t{0});
}
