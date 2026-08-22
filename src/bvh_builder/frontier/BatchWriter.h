#pragma once

#include <cstddef>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"

#include "batch_limits.h"
#include "frontier/Storage.h"
#include "frontier/layout.h"
#include "mem.h"
#include "optional_utils.h"

namespace frontier {

namespace detail {
size_t compute_batch_byte_size(const Batch &batch) {
    return sizeof_val(std::span{batch.entries});
}
} // namespace detail

// Writes frontier entries into batches with a maximum size.
class BatchWriter {
public:
    BatchWriter(Storage &output, const size_t max_batch_bytes = default_max_batch_bytes) : _output(output), _max_batch_bytes(max_batch_bytes) {}

    // Appends `entry`, writing the batch out once it is full.
    tl::expected<void, io::Error> write(const Entry &entry) {
        this->_current.entries.push_back(entry);

        if (detail::compute_batch_byte_size(this->_current) < this->_max_batch_bytes) {
            return {};
        }
        return this->flush();
    }

    // Writes the batch still in progress.
    tl::expected<void, io::Error> flush() {
        if (this->_current.entries.empty()) {
            return {};
        }

        Batch finished = std::move(this->_current);
        return this->_output.write(finished).map([](const auto&) {});
    }

private:
    Storage &_output;
    size_t _max_batch_bytes;
    Batch _current;
};

} // namespace bvh
