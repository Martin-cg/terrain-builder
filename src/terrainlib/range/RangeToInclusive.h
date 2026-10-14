#pragma once

#include "Bound.h"
#include "RangeBounds.h"
#include "numeric/checked_cast.h"

template <typename T>
struct RangeToInclusive : RangeBounds<RangeToInclusive<T>, T> {
    T end;

    constexpr RangeToInclusive(T end_value) : end(end_value) {}

    template <typename U>
    explicit constexpr RangeToInclusive(const RangeToInclusive<U> &other) : RangeToInclusive(debug_checked_cast<T>(other.end)) {}

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::unbounded();
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::included(this->end);
    }

    [[nodiscard]] constexpr bool operator==(const RangeToInclusive &) const noexcept = default;
};
