#pragma once

#include <cstddef>
#include <expected>
#include <ranges>
#include <utility>

#include "io/Error.h"

#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "bvh/utils.h"
#include "mem.h"
#include "storage/BaseBatchWriter.h"
#include "error/Result.h"
#include "error/try.h"

namespace bvh {

// Writes bvh nodes into batches with a maximum size.
class BatchWriter final : public BaseBatchWriter<Storage> {
    using Base = BaseBatchWriter<Storage>;

public:
    using Base::Base;
    using Base::compute_buffered_bytes;

    // Create a new leaf node and write it to the current batch.
    [[nodiscard]]
    alp::Result<Reference, io::Error> write_leaf(NodeHeader header, const dag::Id &id) {
        Batch &current = this->open_batch();
        const NodeIndex node_index = current.add_leaf_node(std::move(header), id);
        const BatchIndex batch_index = current.index();

        ALP_TRY(this->flush_if_full());
        return bvh::Reference{
            .batch = batch_index,
            .node = node_index
        };
    }

    // Create a new inner node and write it to the current batch.
    template <std::ranges::input_range R>
    [[nodiscard]]
    alp::Result<Reference, io::Error> write_inner(NodeHeader header, R &&children) {
        Batch &current = this->open_batch();
        const NodeIndex node_index = current.add_inner_node(std::move(header), std::forward<R>(children));
        const BatchIndex batch_index = current.index();

        ALP_TRY(this->flush_if_full());
        return bvh::Reference{
            .batch = batch_index,
            .node = node_index
        };
    }

protected:
    Batch create_batch(Storage &output) override {
        return output.create_batch();
    }

    size_t compute_buffered_bytes(const Batch &batch) const override {
        return sizeof_val(batch.index(), batch.nodes(), batch.leaves(), batch.children());
    }
};

} // namespace bvh