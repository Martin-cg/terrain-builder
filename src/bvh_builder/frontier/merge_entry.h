#pragma once

#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

#include "range_utils.h"

#include "bvh/layout.h"
#include "bvh/utils.h"
#include "frontier/layout.h"

namespace frontier {

// Merge two frontier entries into an inner node in a batch.
inline bvh::NodeIndex make_inner_node(bvh::Batch &batch, const morton::Code morton, const Entry &a, const Entry &b) {
    const bvh::NodeHeader header = bvh::merge_headers(std::array{a.header, b.header});
    const bvh::NodeIndex node_index = bvh::make_inner_node(batch, header, std::array{a.ref, b.ref});
    return node_index;
}

// Merge a group of frontier entries into one inner node in a batch.
template <std::ranges::forward_range R>
inline bvh::NodeIndex make_inner_node(bvh::Batch &batch, R &&entries) {
    const auto child_headers = entries | std::views::transform(&Entry::header);
    const bvh::NodeHeader header = bvh::merge_headers(child_headers);
    const auto child_eferences = entries | std::views::transform(&Entry::ref);
    const bvh::NodeIndex node_index = bvh::make_inner_node(batch, header, child_references);
    return node_index;
}

// Merge two frontier entries into an inner node in a batch.
inline Entry merge_entries(bvh::Batch &batch, const bvh::BatchIndex batch_index, const morton::Code morton, const Entry &a, const Entry &b) {
    const bvh::NodeHeader header = bvh::merge_headers(std::array{a.header, b.header});
    const bvh::NodeIndex node_index = bvh::make_inner_node(batch, header, std::array{a.ref, b.ref});

    return Entry{
        .morton = morton,
        .header = header,
        .ref = bvh::Reference{.batch = batch_index, .node = node_index},
    };
}

// Merge a group of frontier entries into one inner node in a batch.
template <std::ranges::forward_range R>
inline Entry merge_entries(bvh::Batch &batch, const bvh::BatchIndex batch_index, const morton::Code morton, R &&entries) {
    const auto child_headers = entries | std::views::transform(&Entry::header);
    const bvh::NodeHeader header = bvh::merge_headers(child_headers);
    const auto references = entries | std::views::transform(&Entry::ref);
    const bvh::NodeIndex node_index = bvh::make_inner_node(batch, header, references);

    return Entry{
        .morton = morton,
        .header = header,
        .ref = bvh::Reference{.batch = batch_index, .node = node_index},
    };
}

}
