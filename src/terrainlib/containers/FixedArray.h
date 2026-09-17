#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <ranges>
#include <span>
#include <utility>

// A heap array whose size is fixed at construction, storing one addressable element each, bools included.
template <typename T>
class FixedArray {
public:
    explicit FixedArray(const size_t size)
        : _size(size),
          _data(size == 0 ? nullptr : std::make_unique<T[]>(size)) {}

    FixedArray(const size_t size, const T &value) : FixedArray(size) {
        std::ranges::fill(this->span(), value);
    }

    FixedArray(std::initializer_list<T> values) : FixedArray(values.size()) {
        std::ranges::copy(values, this->begin());
    }

    FixedArray(const FixedArray &other) : FixedArray(other._size) {
        std::ranges::copy(other.span(), this->begin());
    }

    FixedArray &operator=(const FixedArray &other) {
        if (this == &other) {
            return *this;
        }

        FixedArray copy(other);
        *this = std::move(copy);
        return *this;
    }

    FixedArray(FixedArray &&other) noexcept
        : _size(std::exchange(other._size, 0)),
          _data(std::move(other._data)) {}

    FixedArray &operator=(FixedArray &&other) noexcept {
        this->_size = std::exchange(other._size, 0);
        this->_data = std::move(other._data);
        return *this;
    }

    [[nodiscard]]
    size_t size() const noexcept {
        return this->_size;
    }

    [[nodiscard]]
    bool empty() const noexcept {
        return this->_size == 0;
    }

    T &operator[](size_t index) noexcept {
        return this->_data[index];
    }

    const T &operator[](size_t index) const noexcept {
        return this->_data[index];
    }

    T *data() noexcept {
        return this->_data.get();
    }

    const T *data() const noexcept {
        return this->_data.get();
    }

    T *begin() noexcept {
        return this->_data.get();
    }

    const T *begin() const noexcept {
        return this->_data.get();
    }

    T *end() noexcept {
        return this->_data.get() + this->_size;
    }

    const T *end() const noexcept {
        return this->_data.get() + this->_size;
    }

    std::span<T> span() noexcept {
        return {this->_data.get(), this->_size};
    }

    std::span<const T> span() const noexcept {
        return {this->_data.get(), this->_size};
    }

    bool operator==(const FixedArray &other) const {
        return std::ranges::equal(*this, other);
    }

private:
    size_t _size;
    std::unique_ptr<T[]> _data;
};
