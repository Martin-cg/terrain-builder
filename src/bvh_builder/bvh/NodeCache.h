#pragma once

#include <cstdint>
#include <expected>

#include <libassert/assert.hpp>

#include "io/Error.h"
#include "error/Result.h"
#include "error/try.h"

#include "bvh/Storage.h"
#include "bvh/Batch.h"
#include "bvh/Node.h"
#include "bvh/Reference.h"
#include "storage/BatchCache.h"

namespace bvh {

// A cache for the nodes of a tree, addressed by reference.
class NodeCache : public BatchCache<Storage> {
public:
    using BatchCache<Storage>::BatchCache;

    // The node `reference` points at.
    [[nodiscard]]
    alp::Result<Node, io::Error> read_node(const Reference &reference) {
        ALP_TRY(const Batch &batch, this->read_batch(reference.batch));
        ASSERT(reference.node < batch.nodes().size());
        return batch.nodes()[reference.node];
    }
};

} // namespace bvh
