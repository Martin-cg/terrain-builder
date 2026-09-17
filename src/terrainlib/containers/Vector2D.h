#pragma once

#include <cstdint>
#include <vector>

#include "type_utils.h"

template <typename T>
class Vector2D {
public:
    using index_type = uint32_t;

    Vector2D(index_type rows, index_type cols, const T &initial_value = {})
        : _rows(rows), _cols(cols) {
        this->_data.resize(rows * cols, initial_value);
    }

    template <typename Self>
    auto &operator()(this Self &&self, const index_type row, const index_type col) {
        return self._data[(row * self._cols) + col];
    }

    template <typename Self>
    std::span<copy_const_t<Self, T>> get_row(this Self &&self, const index_type row) {
        const index_type start_index = row * self._cols;
        return std::span(self._data.data() + start_index, self._cols);
    }

    index_type rows() const {
        return this->_rows;
    }
    index_type cols() const {
        return this->_cols;
    }
    index_type size() const {
        return this->_data.size();
    }

    bool operator==(const Vector2D &) const = default;

private:
    index_type _rows;
    index_type _cols;
    std::vector<T> _data;
};
