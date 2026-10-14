#pragma once

#include <compare>
#include <cstdint>

#include <fmt/format.h>
#include <zpp_bits.h>

#include "octree/Id.h"

namespace dag {

// The id of a dag group.
struct GroupId {
    octree::Id node;
    uint32_t group_index;

    auto operator<=>(const GroupId &) const = default;
};

} // namespace dag

template <>
struct fmt::formatter<dag::GroupId> {
    constexpr auto parse(fmt::format_parse_context &ctx) {
        return ctx.begin();
    }
    auto format(const dag::GroupId &id, fmt::format_context &ctx) const {
        return fmt::format_to(ctx.out(), "GroupId(node={}, group_index={})", id.node, id.group_index);
    }
};

namespace zpp::bits {

template <typename Archive>
auto serialize(Archive &archive, const dag::GroupId &id) {
    return archive(id.node, id.group_index);
}

template <typename Archive>
auto serialize(Archive &archive, dag::GroupId &id) {
    return archive(id.node, id.group_index);
}

} // namespace zpp::bits
