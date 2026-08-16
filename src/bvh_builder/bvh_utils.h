#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <libassert/assert.hpp>

#include "geometry/geometry.h"

#include "bvh.h"

namespace bvh {

// Aggregate metrics from multiple headers into a parent header.
template <class ResolveHeader>
NodeHeader merge_headers(const size_t child_count, ResolveHeader &&resolve_header) {
    DEBUG_ASSERT(child_count > 0);

    NodeHeader result = resolve_header(0);

    for (size_t i = 1; i < child_count; i++) {
        const NodeHeader &child = resolve_header(i);
        result.min_child_error = std::min(result.min_child_error, child.min_child_error);
        result.max_parent_error = std::max(result.max_parent_error, child.max_parent_error);
        result.triangle_count += child.triangle_count;
        result.bounds = geometry::union_bounds(result.bounds, child.bounds);
    }

    return result;
}

// Merge contiguous child headers into a parent header.
inline NodeHeader merge_headers(const std::span<const NodeHeader> headers) {
    return merge_headers(headers.size(), [&](const size_t index) -> const NodeHeader & {
        return headers[index];
    });
}

// Merge child headers into a parent header.
template <std::same_as<NodeHeader>... Headers>
NodeHeader merge_headers(const Headers &...headers) {
    const std::array<NodeHeader, sizeof...(Headers)> array = {headers...};
    return merge_headers(std::span<const NodeHeader>(array));
}

inline bool is_leaf(const Node &node) {
    return node.child_count == 0;
}

// Append a node to the batch and return its index.
inline NodeIndex emit_node(Batch &batch, Node node) {
    const auto index = static_cast<NodeIndex>(batch.nodes.size());
    batch.nodes.push_back(std::move(node));
    return index;
}

inline NodeIndex emit_leaf_node(Batch &batch, NodeHeader header, const dag::Id id) {
    const auto leaf_start = static_cast<uint32_t>(batch.leaves.size());
    batch.leaves.push_back(id);

    return emit_node(batch, Node{
        .header = std::move(header),
        .child_start = leaf_start,
        .child_count = 0,
    });
}

inline NodeIndex emit_inner_node(Batch &batch, NodeHeader header, const std::span<const Reference> children) {
    DEBUG_ASSERT(!children.empty());

    const auto child_start = static_cast<uint32_t>(batch.children.size());
    batch.children.insert(batch.children.end(), children.begin(), children.end());

    return emit_node(batch, Node{
        .header = std::move(header),
        .child_start = child_start,
        .child_count = static_cast<uint32_t>(children.size()),
    });
}

}
