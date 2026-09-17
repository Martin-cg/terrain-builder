#pragma once

#include <cstdint>
#include <vector>

#include <libassert/assert.hpp>

template <typename T>
struct OffsetVector {
    std::vector<T> data;
    size_t offset = 0;

    OffsetVector() = default;
    OffsetVector(std::vector<T> data, size_t offset = 0)
        : data(std::move(data)), offset(offset) {}

    [[nodiscard]] size_t size() const {
        return this->data.size();
    }
    [[nodiscard]] bool empty() const {
        return this->data.empty();
    }

    void resize(size_t new_size, const T& default_value={}) {
        this->data.resize(new_size, default_value);
    }

    [[nodiscard]] bool contains(size_t index) const {
        return index >= this->offset && index < this->offset + this->size();
    }

    template <typename Self>
    [[nodiscard]] auto &operator[](this Self &&self, size_t index) {
        DEBUG_ASSERT(self.contains(index));
        return self.data[index - self.offset];
    }

    template <typename Self>
    [[nodiscard]] auto begin(this Self &&self) {
        return self.data.begin();
    }
    template <typename Self>
    [[nodiscard]] auto end(this Self &&self) {
        return self.data.end();
    }

    bool operator==(const OffsetVector &) const = default;
};
