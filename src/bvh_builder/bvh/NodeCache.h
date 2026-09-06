#pragma once

#include <cstdint>

#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "io/Error.h"
#include "try.h"

#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "storage/BatchCache.h"

namespace bvh {

// A cache for the nodes of a tree, addressed by reference.
class NodeCache : public BatchCache<Storage> {
public:
    using BatchCache<Storage>::BatchCache;

    // The node `reference` points at.
    tl::expected<Node, io::Error> read_node(const Reference &reference) {
        TRY(const Batch &batch, this->read_batch(reference.batch));
        ASSERT(reference.node < batch.nodes().size());
        return batch.nodes()[reference.node];
    }
};

} // namespace bvh
