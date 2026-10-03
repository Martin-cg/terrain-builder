#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"
#include "try.h"

#include "frontier/Storage.h"
#include "frontier/layout.h"

namespace frontier {

// Reads frontier entries out of a storage's batches in order, holding one batch at a time.
class BatchReader {
public:
    explicit BatchReader(const Storage &input) : _input(&input) {}

    // Takes the next entry, reading the following batch when the current one is used up. Nothing once the storage is exhausted.
    [[nodiscard]] tl::expected<std::optional<Entry>, io::Error> next() {
        while (true) {
            // If we have entries left in the current batch, return the next one.
            if (this->_position < this->_current_batch.entries.size()) {
                auto current = this->_current_batch.entries[this->_position];
                this->_position++;
                return current;
            } else {
                // If we have no more batches to read, return nothing.
                if (this->_next_batch_index == this->_input->size()) {
                    break;
                }

                // Read the next batch.
                TRY(this->_current_batch, this->_input->read(this->_next_batch_index));
                this->_next_batch_index++;
                this->_position = 0;
            }
        }

        return std::nullopt;
    }

private:
    const Storage *_input;
    uint32_t _next_batch_index = 0;
    Batch _current_batch;
    size_t _position = 0;
};

}
