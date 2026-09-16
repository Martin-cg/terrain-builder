#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"
#include "octree/OddLevelShifted.h"

#include "../../dag_builder/storage.h"

#include "bvh/layout.h"
#include "bvh/Storage.h"
#include "bvh/BatchWriter.h"
#include "bvh/utils.h"
#include "traverse_dag.h"

namespace bvh {

struct FlattenOptions {
    size_t max_batch_bytes = default_max_batch_bytes;
    size_t max_resident_bytes = default_max_resident_bytes;
};

// The header a dag node carries as a bvh leaf.
inline NodeHeader make_header(const dag::FlatNode &node) {
    return NodeHeader{
        .min_child_error = node.group.error,
        .max_parent_error = node.parent_error,
        .bounds = node.group.bounds,
    };
}

// Writes every dag node as a set of bvh leaf batches.
inline tl::expected<void, io::Error> flatten_dag(
    const octree::IndexedDagMetaStorage &dag, const octree::OddLevelShifted &shifted_space, Storage &output,
    const FlattenOptions &options = {}) {
    BatchWriter writer(output, options.max_batch_bytes);

    // The walk stops on the first failed write, leaving it to report.
    TRY(dag::traverse_nodes(dag, shifted_space, options.max_resident_bytes, [&](const dag::FlatNode &node) -> tl::expected<void, io::Error> {
        TRY(writer.write_leaf(make_header(node), node.id));
        return {};
    }));
    return writer.flush();
}

}