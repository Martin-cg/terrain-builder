#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <unordered_map>

#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "io/Error.h"
#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "range_utils.h"

#include "build.h"
#include "metadata.h"
#include "../dag_builder/storage.h"
#include "error/Result.h"
#include "error/try.h"
#include "visitor.h"

namespace dag {

struct FlatNode {
    Id id;
    const Group &group;
    uint32_t triangle_count;
    double parent_error;
};

namespace detail {
// A cluster carries the group it belongs to in the node storing it.
inline FlatNode make_flat_node(const Id &id, const NodeMetadata &metadata, const double parent_error) {
    const uint32_t group_index = metadata.group_assignment[id.cluster_index];
    return FlatNode{
        .id = id,
        .group = metadata.groups[group_index],
        .triangle_count = metadata.triangle_counts[id.cluster_index],
        .parent_error = parent_error,
    };
}

inline uint32_t find_coarsest_level(const octree::IndexMap &index) {
    uint32_t coarsest = std::numeric_limits<uint32_t>::max();
    for (const auto &[id, status] : index) {
        if (index.has_data(id)) {
            coarsest = std::min<uint32_t>(coarsest, id.level());
        }
    }
    return coarsest;
}
} // namespace detail

// Visits every cluster in the dag. False if the visitor stopped the walk before its end.
template <typename F>
[[nodiscard]]
alp::Result<bool, io::Error> traverse_nodes(const octree::IndexedDagMetaStorage& dag, F&& v) {
    visitor::FallibleVisitor<io::Error, F&> visitor(v);

    const octree::IndexMap &index = dag.index();
    const uint32_t coarsest_level = detail::find_coarsest_level(index);

    // We count nodes from both sides to ensure we didn't miss any.
    uint64_t cluster_count = 0;
    uint64_t visit_count = 0;

    for (const auto &[parent_id, status] : index) {
        // Skip virtual nodes
        if (!index.has_data(parent_id)) {
            continue;
        }

        ALP_TRY(const auto &parent, dag.load(parent_id));
        cluster_count += parent.group_assignment.size();

        // We assume that every root node is at the coarsest level.
        if (parent_id.level() == coarsest_level) {
            for (const uint32_t cluster_index : range(parent.group_assignment.size())) {
                const Id id{
                    .source_batch = parent_id,
                    .cluster_index = cluster_index};
                const double inf = std::numeric_limits<double>::infinity();
                const FlatNode root = detail::make_flat_node(id, parent, inf);
                visit_count++;
                ALP_TRY(const bool keep_going, visitor(root));
                if (!keep_going) {
                    return false;
                }
            }
        }

        // Handle all children but now the current node (except if root).
        for (const Group &group : parent.groups) {
            for (const Id &child : group.children) {
                ALP_TRY(const auto &metadata, dag.load(child.source_batch));

                const FlatNode node = detail::make_flat_node(child, metadata, group.error);
                visit_count++;
                ALP_TRY(const bool keep_going, visitor(node));
                if (!keep_going) {
                    return false;
                }
            }
        }
    }

    // Every cluster is visited once.
    DEBUG_ASSERT(visit_count == cluster_count);

    // We walked to the end
    return true;
}

}