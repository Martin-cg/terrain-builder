#pragma once

#include <cstddef>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"

#include "batch_limits.h"
#include "containers/UniqueResource.h"
#include "frontier/Storage.h"
#include "frontier/layout.h"
#include "log.h"
#include "mem.h"
#include "optional_utils.h"
#include "try.h"

namespace frontier {

namespace detail {
inline size_t compute_batch_byte_size(const Batch &batch) {
    return sizeof_val(std::span{batch.entries});
}

// A batch being filled, together with the run it belongs to.
struct PendingBatch {
    // Writes a batch that its writer left open.
    struct Flush {
        void operator()(PendingBatch &pending) const noexcept {
            LOG_WARN("Batch was not written upon writer destruction, use flush().");

            const auto result = pending.output->write(pending.batch);
            if (!result) {
                LOG_ERROR("Failed to write a frontier batch: {}", result.error());
            }
        }
    };

    Storage *output;
    Batch batch;
};
} // namespace detail

// Writes frontier entries into batches with a maximum size.
class BatchWriter {
public:
    BatchWriter(Storage &output, const size_t max_batch_bytes = default_max_batch_bytes) : _output(output), _max_batch_bytes(max_batch_bytes) {}

    // Appends `entry`, writing the batch out once it is full.
    tl::expected<void, io::Error> write(const Entry &entry) {
        if (this->_current.is_empty()) {
            this->_current.reset(detail::PendingBatch{.output = &this->_output, .batch = {}});
        }
        this->_current->batch.entries.push_back(entry);

        if (detail::compute_batch_byte_size(this->_current->batch) < this->_max_batch_bytes) {
            return {};
        }
        return this->flush();
    }

    // Writes the batch still in progress.
    tl::expected<void, io::Error> flush() {
        if (this->_current.is_empty()) {
            return {};
        }

        TRY(this->_output.write(this->_current->batch));
        this->_current.take();
        return {};
    }

private:
    Storage &_output;
    size_t _max_batch_bytes;
    UniqueResource<detail::PendingBatch, detail::PendingBatch::Flush> _current;
};

} // namespace bvh
