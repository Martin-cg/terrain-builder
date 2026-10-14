#pragma once

#include "Bound.h"
#include "RangeBounds.h"
#include "numeric/checked_cast.h"

template <typename T>
struct RangeFrom : RangeBounds<RangeFrom<T>, T> {
    T start;

    constexpr RangeFrom(T start_value) : start(start_value) {}

    template <typename U>
    explicit constexpr RangeFrom(const RangeFrom<U> &other) : RangeFrom(debug_checked_cast<T>(other.start)) {}

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::included(this->start);
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::unbounded();
    }

    [[nodiscard]] constexpr bool operator==(const RangeFrom &) const noexcept = default;
};
