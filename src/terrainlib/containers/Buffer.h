#pragma once

#include <algorithm>
#include <array>
#include <initializer_list>
#include <memory>
#include <span>

#include <libassert/assert.hpp>

#include "containers/Size.h"
#include "type_utils.h"

template <typename T, Size S>
class BufferStorage;

template <typename T, size_t N>
class BufferStorage<T, ComptimeSize<N>> {
public:
    constexpr size_t size() const noexcept {
        return N;
    }

    template <typename Self>
    constexpr auto *data(this Self &&self) noexcept {
        return self.storage.data();
    }

    template <typename Self>
    constexpr auto span(this Self &&self) noexcept {
        return std::span(self.storage);
    }

private:
    std::array<T, N> storage{};
};

template <typename T>
class BufferStorage<T, RuntimeSize> {
public:
    explicit BufferStorage(const size_t n)
        : storage(std::make_unique<T[]>(n)), n(n) {}

    size_t size() const noexcept {
        return this->n;
    }

    template <typename Self>
    copy_const_t<Self, T> *data(this Self &&self) noexcept {
        return self.storage.get();
    }

    template <typename Self>
    auto span(this Self &&self) noexcept {
        return std::span(self.data(), self.size());
    }

private:
    std::unique_ptr<T[]> storage;
    size_t n;
};

template <typename T, Size S>
class Buffer {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T *;
    using const_pointer = const T *;
    using reference = T &;
    using const_reference = const T &;
    using iterator = T *;
    using const_iterator = const T *;

    constexpr Buffer()
        requires(S::is_comptime())
    = default;

    explicit Buffer(const size_t n)
        requires(S::is_runtime())
        : storage(n) {}

    Buffer(std::initializer_list<T> values)
        requires(S::is_runtime())
        : storage(values.size()) {
        std::ranges::copy(values, this->begin());
    }

    constexpr size_t size() const noexcept {
        return this->storage.size();
    }
    constexpr bool empty() const noexcept {
        return this->size() == 0;
    }

    template <typename Self>
    constexpr auto *data(this Self &&self) noexcept {
        return self.storage.data();
    }

    template <typename Self>
    constexpr auto span(this Self &&self) noexcept {
        return self.storage.span();
    }

    template <typename Self>
    constexpr auto &operator[](this Self &&self, const size_t i) noexcept {
        DEBUG_ASSERT(i < self.size());
        return self.data()[i];
    }

    template <typename Self>
    auto &at(this Self &&self, const size_t i) {
        if (i >= self.size()) {
            throw std::out_of_range("Buffer::at");
        }
        return self.data()[i];
    }

    template <typename Self>
    auto &front(this Self &&self) {
        return self.at(0);
    }
    template <typename Self>
    auto &back(this Self &&self) {
        return self.at(self.size() - 1);
    }

    template <typename Self>
    constexpr auto *begin(this Self &&self) noexcept {
        return self.data();
    }
    constexpr const_iterator cbegin() const noexcept {
        return this->data();
    }

    template <typename Self>
    constexpr auto *end(this Self &&self) noexcept {
        return self.data() + self.size();
    }
    constexpr const_iterator cend() const noexcept {
        return this->data() + this->size();
    }

    bool operator==(const Buffer &other) const {
        return std::ranges::equal(*this, other);
    }

private:
    BufferStorage<T, S> storage;
};

inline constexpr size_t MAX_STACK_BYTES = 1024;

template <typename T>
inline constexpr Buffer<T, RuntimeSize> make_heap_buffer(const size_t size) {
    return Buffer<T, RuntimeSize>(size);
}
template <typename T, size_t N>
inline constexpr Buffer<T, ComptimeSize<N>> make_stack_buffer() {
    return Buffer<T, ComptimeSize<N>>();
}

template <class T, Size S>
auto make_buffer(const S s) {
    if constexpr (S::is_comptime()) {
        constexpr size_t N = S::value();
        if constexpr (sizeof(T) * N < MAX_STACK_BYTES) {
            return make_stack_buffer<T, N>();
        } else {
            return make_heap_buffer<T>(N);
        }
    } else {
        return make_heap_buffer<T>(s);
    }
}