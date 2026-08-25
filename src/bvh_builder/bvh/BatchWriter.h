#pragma once

#include <cstddef>

#include <tl/expected.hpp>

#include "io/Error.h"

#include "bvh/layout.h"
#include "bvh/Storage.h"
#include "bvh/utils.h"
#include "containers/UniqueResource.h"
#include "log.h"
#include "batch_limits.h"
#include "mem.h"
#include "try.h"

namespace bvh {

namespace detail {

inline size_t compute_batch_byte_size(const Batch& batch) {
    return sizeof_val(batch.index(), batch.nodes(), batch.leaves(), batch.children());
}

// A batch being filled, together with the storage it belongs to.
struct PendingBatch {
    // Writes a batch that its writer left open.
    struct Flush {
        void operator()(PendingBatch &pending) const noexcept {
            LOG_WARN("Batch was not written upon writer destruction, use flush().");

            const auto result = pending.output->write(pending.batch);
            if (!result) {
                LOG_ERROR("Failed to write batch {}: {}", pending.batch.index(), result.error());
            }
        }
    };

    Storage *output;
    Batch batch;
};

} // namespace detail

// Writes bvh nodes into batches with a maximum size.
class BatchWriter {
public:
    BatchWriter(Storage &output, const size_t max_batch_bytes = default_max_batch_bytes) : _output(output), _max_batch_bytes(max_batch_bytes) {}

    // Create a new leaf node and write it to the current batch.
    tl::expected<Reference, io::Error> write_leaf(NodeHeader header, const dag::Id &id) {
        Batch &current = this->current_batch();
        const NodeIndex node_index = current.add_leaf_node(std::move(header), id);
        const BatchIndex batch_index = current.index();

        TRY(this->flush_if_full());
        return bvh::Reference{
            .batch = batch_index,
            .node = node_index
        };
    }

    // Create a new inner node and write it to the current batch.
    template <std::ranges::input_range R>
    tl::expected<Reference, io::Error> write_inner(NodeHeader header, R &&children) {
        Batch &current = this->current_batch();
        const NodeIndex node_index = current.add_inner_node(std::move(header), std::forward<R>(children));
        const BatchIndex batch_index = current.index();

        TRY(this->flush_if_full());
        return bvh::Reference{
            .batch = batch_index,
            .node = node_index
        };
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
    Batch &current_batch() {
        if (this->_current.is_empty()) {
            this->_current.reset(detail::PendingBatch{
                .output = &this->_output,
                .batch = this->_output.create_batch(),
            });
        }
        return this->_current->batch;
    }

    tl::expected<void, io::Error> flush_if_full() {
        if (this->_current.is_empty()) {
            return {};
        }

        if (detail::compute_batch_byte_size(this->_current->batch) < this->_max_batch_bytes) {
            return {};
        }
        return this->flush();
    }

    Storage &_output;
    size_t _max_batch_bytes;
    UniqueResource<detail::PendingBatch, detail::PendingBatch::Flush> _current;
};

}
