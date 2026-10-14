#pragma once

#include "Bound.h"
#include "RangeBounds.h"
#include "numeric/checked_cast.h"

template <typename T>
struct RangeTo : RangeBounds<RangeTo<T>, T> {
    T end;

    constexpr RangeTo(T end_value) : end(end_value) {}

    template <typename U>
    explicit constexpr RangeTo(const RangeTo<U> &other) : RangeTo(debug_checked_cast<T>(other.end)) {}

    [[nodiscard]] constexpr Bound<T> start_bound() const noexcept {
        return Bound<T>::unbounded();
    }

    [[nodiscard]] constexpr Bound<T> end_bound() const noexcept {
        return Bound<T>::excluded(this->end);
    }

    [[nodiscard]] constexpr bool operator==(const RangeTo &) const noexcept = default;
};
