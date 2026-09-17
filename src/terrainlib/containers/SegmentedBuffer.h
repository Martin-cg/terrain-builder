#pragma once

#include <algorithm>
#include <cstddef>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <libassert/assert.hpp>

#include "containers/OffsetTable.h"
#include "type_utils.h"

// Contiguous buffer partitioned into segments using an OffsetTable for range-based indexing.
template <typename TValue, typename TValueIndex = size_t, typename TSegmentIndex = TValueIndex>
class SegmentedBuffer {
public:
    using value_type = TValue;
    using index_type = TValueIndex;
    using segment_index = TSegmentIndex;
    using offset_range = typename OffsetTable_<index_type>::range_type;

    SegmentedBuffer() {
        this->reset();
    }
    SegmentedBuffer(std::vector<value_type> segment) : _data(std::move(segment)) {
        this->_offsets.append_length(this->_data.size());
    }

    // Resets the buffer to its default state.
    void reset() {
        this->_offsets.clear();
        this->_data.clear();
    }

    // Initializes the buffer with pre-defined segment sizes and pre-allocates backing storage.
    void init(const std::span<const index_type> segment_sizes, const value_type &value = {}) {
        if (segment_sizes.empty()) {
            this->reset();
            return;
        }

        this->_offsets.clear();
        this->_data.clear();

        this->_offsets.append_lengths(segment_sizes);
        this->_data.resize(this->_offsets.total_size(), value);
    }

    // Reserve size in the global backing storage.
    void reserve(const index_type size) {
        this->_data.reserve(size);
    }

    // Resize the last segment.
    void resize_last_segment(const index_type size, const value_type &value = {}) {
        this->ensure_segment();
        if (size == 0) {
            return;
        }

        const segment_index last_segment = this->segment_count() - 1;
        const index_type new_end = this->_offsets.segment_range(last_segment).begin + size;
        this->_offsets.set_end(last_segment, new_end);
        this->_data.resize(new_end, value);
    }

    // Adds a new empty segment with a pre-allocated capacity.
    void push_new_segment(const index_type size, const value_type &value = {}) {
        this->start_new_segment();
        this->resize_last_segment(size, value);
    }

    // Pushes a new segment and copies all elements from a container into it.
    template <std::ranges::input_range Range>
    void push_new_segment(const Range& range) {
        // resize
        const index_type added_size = std::ranges::size(range);
        this->push_new_segment(added_size);

        // copy data
        const segment_index last_segment = this->segment_count() - 1;
        auto destination = this->segment(last_segment);
        std::ranges::copy(range, destination.begin());
    }

    // Appends an item to the end of the last segment in the data buffer.
    void push_to_last_segment(const value_type &value) {
        this->ensure_segment();
        const segment_index last_segment = this->segment_count() - 1;
        const offset_range range = this->_offsets.segment_range(last_segment);

        this->_data.push_back(value);
        this->_offsets.set_end(last_segment, range.end + 1);
    }

    // Appends an item to the end of the last segment in the data buffer.
    void push_to_last_segment(value_type &&value) {
        this->ensure_segment();
        const segment_index last_segment = this->segment_count() - 1;
        const offset_range range = this->_offsets.segment_range(last_segment);

        this->_data.push_back(std::forward<value_type>(value));
        this->_offsets.set_end(last_segment, range.end + 1);
    }

    // Finalizes the current segment and starts a new one at the current buffer position.
    void start_new_segment() {
        if (this->segment_count() > 0) {
            this->_offsets.set_end(this->segment_count() - 1, this->total_size());
        }
        this->_offsets.append_length(0);
    }

    // Removes all empty segments while preserving the order of non-empty segments.
    // If all segments are empty, the buffer is reset to one empty segment.
    void remove_empty_segments() {
        this->_offsets.remove_empty_segments();
    }

    // Accesses an element using segment-relative indexing.
    template <typename Self>
    auto &operator()(this Self &&self, const segment_index segment_index, const index_type element_index) {
        DEBUG_ASSERT(segment_index < self._offsets.size());

        const offset_range range = self._offsets.segment_range(segment_index);
        const index_type flat_index = range.begin + element_index;

        DEBUG_ASSERT(flat_index < range.end);
        return self._data[flat_index];
    }

    // Returns the total number of segments currently tracked.
    segment_index segment_count() const noexcept {
        return this->_offsets.size();
    }

    // Returns the number of elements contained within a specific segment.
    index_type segment_size(const segment_index segment_index) const noexcept {
        return this->_offsets.segment_size(segment_index);
    }

    // Returns the total number of elements across all segments.
    index_type total_size() const noexcept {
        return this->_data.size();
    }

    // Returns a view of one segment of the buffer, read-only when the buffer is.
    template <typename Self>
    std::span<copy_const_t<Self, value_type>> segment(this Self &self, const segment_index segment_index) noexcept {
        DEBUG_ASSERT(segment_index < self._offsets.size());

        const offset_range range = self._offsets.segment_range(segment_index);
        const index_type length = range.end - range.begin;
        return std::span(self._data.data() + range.begin, length);
    }

    // Returns a view of the last segment of the buffer, read-only when the buffer is.
    // Empty while there is none.
    template <typename Self>
    std::span<copy_const_t<Self, value_type>> last_segment(this Self &self) noexcept {
        if (self.segment_count() == 0) {
            return {};
        }
        return self.segment(self.segment_count() - 1);
    }

    // Returns a flat view of the buffer, read-only when the buffer is.
    template <typename Self>
    std::span<copy_const_t<Self, value_type>> flat(this Self &self) noexcept {
        return self._data;
    }

    template <typename Self>
    auto segments(this Self &self) noexcept {
        return std::views::iota(segment_index{0}, self.segment_count())
             | std::views::transform([&self](const segment_index i) noexcept {
                   return self.segment(i);
               });
    }

    template <typename Self>
    auto &backing(this Self &&self) noexcept {
        return self._data;
    }

    bool operator==(const SegmentedBuffer &) const = default;

private:
    void ensure_segment() {
        if (this->segment_count() == 0) {
            this->start_new_segment();
        }
    }

    // Manages the boundaries of each segment.
    OffsetTable_<index_type> _offsets;

    // The contiguous backing storage for all segments.
    std::vector<value_type> _data;
};
