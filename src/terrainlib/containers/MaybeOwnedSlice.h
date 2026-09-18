#pragma once

#include <algorithm>
#include <span>
#include <utility>
#include <variant>
#include <vector>

// A contiguous range of `T`, either owned or borrowed.
template <typename T>
struct MaybeOwnedSlice {
    std::variant<std::vector<T>, std::span<const T>> data;

    MaybeOwnedSlice(std::vector<T> &&values) : data(std::move(values)) {}
    MaybeOwnedSlice(std::span<const T> values) : data(values) {}

    static MaybeOwnedSlice<T> from_owned(std::vector<T> &&values) {
        return MaybeOwnedSlice<T>(std::move(values));
    }
    static MaybeOwnedSlice<T> from_ref(std::span<const T> values) {
        return MaybeOwnedSlice<T>(values);
    }

    [[nodiscard]] bool is_owned() const noexcept {
        return std::holds_alternative<std::vector<T>>(this->data);
    }
    [[nodiscard]] bool is_ref() const noexcept {
        return std::holds_alternative<std::span<const T>>(this->data);
    }

    // The values, whether they are owned or borrowed.
    [[nodiscard]] std::span<const T> get() const {
        return std::visit([](const auto &values) -> std::span<const T> { return values; }, this->data);
    }

    // Two slices are equal when they hold the same values, whether they own them or not.
    [[nodiscard]] bool operator==(const MaybeOwnedSlice<T> &other) const {
        return std::ranges::equal(this->get(), other.get());
    }
};
