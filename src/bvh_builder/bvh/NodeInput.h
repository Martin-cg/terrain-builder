#pragma once

#include <concepts>
#include <ranges>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "type_utils.h"

#include "bvh/NodeHeader.h"
#include "bvh/NodeView.h"

namespace bvh {

// An inner node's header and children, before a batch stores them.
template <std::ranges::input_range R>
struct InnerInput {
    NodeHeader header;
    R children;

    bool operator==(const InnerInput &) const = default;
};

template <typename R>
InnerInput(NodeHeader, R &&) -> InnerInput<std::views::all_t<R>>;

// A leaf's header and cluster records, before a batch stores them.
template <std::ranges::input_range R>
struct LeafInput {
    NodeHeader header;
    R clusters;

    bool operator==(const LeafInput &) const = default;
};

template <typename R>
LeafInput(NodeHeader, R &&) -> LeafInput<std::views::all_t<R>>;

// A node a batch can store.
template <typename N>
concept NodeInput = is_specialization_of_v<N, InnerInput> || is_specialization_of_v<N, LeafInput> || std::same_as<N, NodeView>;

}

template <typename R>
struct fmt::formatter<bvh::LeafInput<R>> {
    constexpr auto parse(fmt::format_parse_context &ctx) {
        return ctx.begin();
    }
    auto format(const bvh::LeafInput<R> &leaf, fmt::format_context &ctx) const {
        return fmt::format_to(ctx.out(), "LeafInput(header={}, clusters={})", leaf.header, leaf.clusters);
    }
};
