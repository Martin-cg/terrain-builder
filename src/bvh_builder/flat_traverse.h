#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

#include <tl/expected.hpp>

#include "enumerate.h"
#include "io/Error.h"
#include "range_utils.h"

#include "bvh.h"
#include "storage.h"

namespace bvh {

// A visitor returning false stops the walk.
template <typename Visitor>
concept FlatVisitor = std::invocable<Visitor, const Reference &, const Node &> &&
    (std::same_as<std::invoke_result_t<Visitor, const Reference &, const Node &>, void> ||
     std::same_as<std::invoke_result_t<Visitor, const Reference &, const Node &>, bool>);

namespace detail {

template <FlatVisitor Visitor>
bool invoke(Visitor &visitor, const Reference &ref, const Node &node) {
    if constexpr (std::same_as<std::invoke_result_t<Visitor, const Reference &, const Node &>, void>) {
        visitor(ref, node);
        return true;
    } else {
        return visitor(ref, node);
    }
}

}

// Visits every stored node with the reference naming it. False if the visitor stopped the walk before its end.
template <FlatVisitor Visitor>
tl::expected<bool, io::Error> flat_traverse(const Storage &nodes, Visitor &&visitor) {
    for (const BatchIndex batch_index : range(nodes.size())) {
        const auto result = nodes.read(batch_index);
        if (!result) {
            return tl::unexpected(result.error());
        }

        for (const auto &[node_index, node] : enumerate<NodeIndex>(result.value().nodes)) {
            const Reference ref{.batch = batch_index, .node = node_index};
            if (!detail::invoke(visitor, ref, node)) {
                return false;
            }
        }
    }

    // We walked to the end
    return true;
}

}
