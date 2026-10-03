#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "containers/FixedVector.h"

// A sum of up to `capacity` floating-point values, held without rounding.
// Requires every value and product to stay within the normal floating-point range.
template <typename T, size_t capacity>
class ExactSum {
public:
    // Adds `value` to the sum.
    void add(const T value) {
        // Grow-Expansion from Shewchuk, "Adaptive Precision Floating-Point Arithmetic and Fast Robust Geometric Predicates", Theorem 10.
        // Each term keeps only the roundoff of adding the carry to it, so the terms always add up to the true sum and never overlap.
        T carry = value;
        for (T &term : this->_terms) {
            const T sum = carry + term;
            const T term_virtual = sum - carry;
            const T carry_virtual = sum - term_virtual;
            term = (carry - carry_virtual) + (term - term_virtual);
            carry = sum;
        }
        this->_terms.push_back(carry);
    }

    // Adds the product of `a` and `b` to the sum.
    // Counts as two values.
    void add_product(const T a, const T b) {
        // fma rounds once, so it yields exactly what the multiplication rounded away.
        const T product = a * b;
        this->add(std::fma(a, b, -product));
        this->add(product);
    }

    // Checks whether the sum is exactly zero.
    bool is_zero() const {
        // Terms that do not overlap cannot cancel each other.
        return std::ranges::all_of(this->_terms, [](const T term) { return term == T(0); });
    }

    // Rounds the sum to a single value.
    T estimate() const {
        T estimate = T(0);
        for (const T term : this->_terms) {
            estimate += term;
        }
        return estimate;
    }

private:
    FixedVector<T, capacity> _terms;
};
