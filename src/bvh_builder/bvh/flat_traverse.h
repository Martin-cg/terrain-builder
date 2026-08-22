#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

#include <tl/expected.hpp>

#include "enumerate.h"
#include "io/Error.h"
#include "range_utils.h"
#include "bvh/layout.h"
#include "bvh/Storage.h"
#include "visitor.h"
#include "try.h"

namespace bvh {

// Visits every stored node with the reference naming it. False if the visitor stopped the walk before its end.
template <typename F>
tl::expected<bool, io::Error> flat_traverse(const Storage &nodes, F &&v) {
    visitor::FallibleVisitor<io::Error, F &> visitor(v);

    for (const BatchIndex batch_index : range(nodes.size())) {
        TRY(const Batch batch, nodes.read(batch_index));

        for (const auto &[node_index, node] : enumerate<NodeIndex>(batch.nodes())) {
            const Reference ref{.batch = batch_index, .node = node_index};
            TRY(const bool keep_going, visitor(ref, node));
            if (!keep_going) {
                return false;
            }
        }
    }

    // We walked to the end
    return true;
}

}
