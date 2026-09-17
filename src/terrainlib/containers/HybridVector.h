#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "containers/FixedVector.h"

template <typename T, size_t N>
class HybridVector {
public:
    HybridVector() = default;

    HybridVector(std::initializer_list<T> values) : HybridVector() {
        this->reserve(values.size());
        for (const T &value : values) {
            this->push_back(value);
        }
    }

    template <typename U>
    void push_back(U &&value) {
        this->emplace_back(std::forward<U>(value));
    }

    template <typename... Args>
    void emplace_back(Args &&...args) {
        if (this->_is_on_stack) {
            if (this->_stack.full()) {
                this->switch_to_heap();
                this->_heap.emplace_back(std::forward<Args>(args)...);
            } else {
                this->_stack.emplace_back(std::forward<Args>(args)...);
            }
        } else {
            this->_heap.emplace_back(std::forward<Args>(args)...);
        }
    }

    void pop_back() {
        if (this->empty()) {
            throw std::out_of_range("HybridVector::pop_back on empty");
        }

        if (this->_is_on_stack) {
            this->_stack.pop_back();
        } else {
            this->_heap.pop_back();
        }
    }

    void clear() {
        if (this->_is_on_stack) {
            this->_stack.clear();
        } else {
            this->_heap.clear();
        }
    }

    void reserve(size_t new_capacity) {
        if (this->_is_on_stack) {
            if (new_capacity > N) {
                this->switch_to_heap(new_capacity);
            }
        } else {
            this->_heap.reserve(new_capacity);
        }
    }

    void resize(size_t new_size) {
        if (this->_is_on_stack) {
            if (new_size > N) {
                this->switch_to_heap(new_size);
                this->_heap.resize(new_size);
            } else {
                this->_stack.resize(new_size);
            }
        } else {
            this->_heap.resize(new_size);
        }
    }

    template <typename Self>
    auto *data(this Self &&self) {
        return self._is_on_stack ? self._stack.data() : self._heap.data();
    }

    size_t size() const {
        return this->_is_on_stack ? this->_stack.size() : this->_heap.size();
    }
    size_t capacity() const {
        return this->_is_on_stack ? this->_stack.capacity() : this->_heap.capacity();
    }
    bool empty() const {
        return this->size() == 0;
    }
    // Whether the elements have moved from the inline storage to the heap.
    bool spilled() const {
        return !this->_is_on_stack;
    }

    template <typename Self>
    auto &operator[](this Self &&self, size_t i) {
        return self._is_on_stack ? self._stack[i] : self._heap[i];
    }

    template <typename Self>
    auto &at(this Self &&self, size_t index) {
        return self._is_on_stack ? self._stack.at(index) : self._heap.at(index);
    }

    template <typename Self>
    auto *begin(this Self &&self) {
        return self.data();
    }
    template <typename Self>
    auto *end(this Self &&self) {
        return self.data() + self.size();
    }
    const T* cbegin() const {
        return this->begin();
    }
    const T* cend() const {
        return this->end();
    }

    operator std::span<T>() {
        return std::span<T>(this->data(), this->size());
    }
    operator std::span<const T>() const {
        return std::span<const T>(this->data(), this->size());
    }

    bool operator==(const HybridVector &other) const {
        return std::ranges::equal(*this, other);
    }

private:
    void switch_to_heap(const size_t reserve_size = N * 2) {
        this->_heap.reserve(reserve_size);
        for (auto &v : this->_stack) {
            this->_heap.push_back(std::move(v));
        }
        this->_stack.clear();
        this->_is_on_stack = false;
    }

    // TODO: use std::variant
    FixedVector<T, N> _stack;
    std::vector<T> _heap;
    bool _is_on_stack = true;
};
