#pragma once

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>

#include <fmt/format.h>

#include "error/Result.h"
#include "error/try.h"
#include "error/utils.h"
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
    size_t max_batch_bytes = DEFAULT_MAX_BATCH_BYTES;
    size_t max_resident_bytes = DEFAULT_MAX_RESIDENT_BYTES;
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
[[nodiscard]]
inline alp::Result<void, io::Error> flatten_dag(
    const octree::IndexedDagMetaStorage &dag, const octree::OddLevelShifted &shifted_space, Storage &output,
    const FlattenOptions &options = {}) {
    return alp::run_with_context(
        alp::make_context("flattening a dag from {} to {}", dag.base_path(), output.directory()),
        [&] -> alp::Result<void, io::Error> {
            BatchWriter writer(output, options.max_batch_bytes);

            // The walk stops on the first failed write, leaving it to report.
            ALP_TRY(dag::traverse_nodes(dag, shifted_space, options.max_resident_bytes, [&](const dag::FlatNode &node) -> alp::Result<void, io::Error> {
                ALP_TRY(writer.write_leaf(make_header(node), node.id));
                return {};
            }));
            return writer.flush();
        });
}

}