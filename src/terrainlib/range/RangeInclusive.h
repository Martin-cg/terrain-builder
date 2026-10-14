#pragma once

#include "Bound.h"
#include "RangeBounds.h"
#include "numeric/checked_cast.h"

template <typename T>
struct RangeInclusive : RangeBounds<RangeInclusive<T>, T> {
    T start;
    T end;

    constexpr RangeInclusive(T value) : RangeInclusive(value, value) {}
    constexpr RangeInclusive(T start_value, T end_value) : start(start_value), end(end_value) {}

    template <typename U>
    explicit constexpr RangeInclusive(const RangeInclusive<U> &other)
        : RangeInclusive(debug_checked_cast<T>(other.start), debug_checked_cast<T>(other.end)) {}

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::included(this->start);
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::included(this->end);
    }

    [[nodiscard]] constexpr bool operator==(const RangeInclusive &) const noexcept = default;
};
