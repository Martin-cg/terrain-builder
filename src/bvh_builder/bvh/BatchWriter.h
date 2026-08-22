#pragma once

#include <cstddef>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"

#include "bvh/layout.h"
#include "bvh/Storage.h"
#include "bvh/utils.h"
#include "optional_utils.h"
#include "batch_limits.h"
#include "mem.h"
#include "try.h"

namespace bvh {

namespace detail {
size_t compute_batch_byte_size(const Batch& batch) {
    return sizeof_val(batch.index(), batch.nodes(), batch.leaves(), batch.children());
}
}

// Writes bvh nodes into batches with a maximum size.
class BatchWriter {
public:
    BatchWriter(Storage &output, const size_t max_batch_bytes = default_max_batch_bytes) : _output(output), _max_batch_bytes(max_batch_bytes) {}

    // Create a new leaf node and write it to the current batch.
    tl::expected<Reference, io::Error> write_leaf(NodeHeader header, const dag::Id &id) {
        Batch &current = this->current_batch();
        const NodeIndex node_index = current.add_leaf_node(std::move(header), id);
        TRY(this->flush_if_full());
        return bvh::Reference{
            .batch = current.index(),
            .node = node_index
        };
    }

    // Create a new inner node and write it to the current batch.
    template <std::ranges::input_range R>
    tl::expected<Reference, io::Error> write_inner(NodeHeader header, R &&children) {
        Batch &current = this->current_batch();
        const NodeIndex node_index = current.add_inner_node(std::move(header), std::forward<R>(children));
        TRY(this->flush_if_full());
        return bvh::Reference{
            .batch = current.index(),
            .node = node_index
        };
    }

    // Writes the batch still in progress.
    tl::expected<void, io::Error> flush() {
        if (!this->_current.has_value()) {
            return {};
        }
        Batch &current = this->_current.value();

        TRY(this->_output.write(current));
        this->_current.reset();
        return {};
    }

private:
    Batch &current_batch() {
        return get_or_insert_with(this->_current, [&]() {
            return this->_output.create_batch();
        });
    }

    tl::expected<void, io::Error> flush_if_full() {
        if (!this->_current.has_value()) {
            return {};
        }
        Batch &current = this->_current.value();

        if (detail::compute_batch_byte_size(current) < this->_max_batch_bytes) {
            return {};
        }
        return this->flush();
    }

    Storage &_output;
    size_t _max_batch_bytes;
    std::optional<Batch> _current;
};

}
