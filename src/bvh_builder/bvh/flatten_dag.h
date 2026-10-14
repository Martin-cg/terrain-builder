#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "error/Result.h"
#include "error/try.h"
#include "error/utils.h"
#include "io/Error.h"
#include "octree/Id.h"
#include "octree/IndexMap.h"
#include "range_utils.h"

#include "../../dag_builder/storage.h"

#include "bvh/NodeHeader.h"
#include "bvh/NodeInput.h"
#include "bvh/Storage.h"
#include "bvh/BatchWriter.h"
#include "dag/traverse_dag.h"

namespace bvh {

struct FlattenOptions {
    size_t max_batch_bytes = DEFAULT_MAX_BATCH_BYTES; // the most bytes a written batch holds
    size_t max_resident_bytes = DEFAULT_MAX_RESIDENT_BYTES; // the most bytes of dag node metadata kept cached
};

namespace detail {

// Finds the coarsest level of `index` holding data.
inline octree::Id::Level find_min_level(const octree::IndexMap &index) {
    octree::Id::Level result = octree::Id::max_level();
    for (const auto &[id, status] : index) {
        if (index.has_data(id)) {
            result = std::min(result, id.level());
        }
    }
    return result;
}

// Makes the leaf holding the child clusters of `group`, or its parent clusters if `is_root`.
inline LeafInput<std::span<const dag::ClusterInfo>> make_leaf(const dag::GroupView &group, const bool is_root) {
    const std::span<const dag::ClusterInfo> clusters = is_root ? group.parent_clusters : group.child_clusters;
    DEBUG_ASSERT(!clusters.empty());

    radix::geometry::Aabb3d tight_bounds;
    for (const dag::ClusterInfo &cluster : clusters) {
        tight_bounds.expand_by(cluster.tight_bounds);
    }

    const NodeHeader header{
        .min_child_error = min(clusters, &dag::ClusterInfo::error),
        .max_parent_error = is_root ? ROOT_PARENT_ERROR : group.error,
        .lod_bounds = group.bounds,
        .tight_bounds = tight_bounds,
    };
    return LeafInput{header, clusters};
}

} // namespace detail

// Writes the leaves of a bvh over `dag` into `output`.
[[nodiscard]]
inline alp::Result<void, io::Error> flatten_dag(const octree::IndexedDagMetaStorage &dag, Storage &output, const FlattenOptions &options = {}) {
    return alp::run_with_context(
        alp::make_context("flattening a dag from {} to {}", dag.base_path(), output.directory()),
        [&] -> alp::Result<void, io::Error> {
            BatchWriter writer(output, options.max_batch_bytes);
            const octree::Id::Level min_level = detail::find_min_level(dag.index());

            // Every cluster is a parent cluster of exactly one group, so the two counts meet once every cluster below the coarsest level is listed exactly once.
            uint64_t child_count = 0;
            uint64_t below_root_count = 0;

            ALP_TRY(dag::traverse_groups(dag, options.max_resident_bytes, [&](const dag::GroupView &group) -> alp::Result<void, io::Error> {
                const bool is_root_level = group.id.node.level() == min_level;
                child_count += group.child_clusters.size();
                if (!is_root_level) {
                    below_root_count += group.parent_clusters.size();
                }

                // The clusters a group lists form one leaf.
                if (!group.child_clusters.empty()) {
                    ALP_TRY(writer.write_node(detail::make_leaf(group, false)));
                }
                // Nothing replaces the clusters a group on the coarsest level produced, so they form a leaf of their own.
                if (is_root_level) {
                    ALP_TRY(writer.write_node(detail::make_leaf(group, true)));
                }
                return {};
            }));

            ASSERT(child_count == below_root_count, "a group lists a cluster on the coarsest level, or a cluster below it is listed other than once");
            return writer.flush();
        });
}

}
