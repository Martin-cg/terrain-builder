#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>

#include <libassert/assert.hpp>
#include <tl/expected.hpp>

#include "io/Error.h"
#include "try.h"

#include "bvh/NodeCache.h"
#include "bvh/Storage.h"
#include "bvh/layout.h"
#include "widen/options.h"
#include "widen/tabulate_splits.h"
#include "widen/write_wide_nodes.h"

namespace widen {

// Writes the tree at `root` into `output` with up to `arity` children per node, and returns its new root.
[[nodiscard]]
inline tl::expected<bvh::Reference, io::Error> build(
    const bvh::Storage &input, const bvh::Reference &root, bvh::Storage &output,
    const std::filesystem::path &scratch, const Options &options) {

    const auto max_resident_batches = static_cast<uint32_t>(std::max<size_t>(1, options.max_resident_bytes / options.max_batch_bytes));

    bvh::NodeCache tree(input, max_resident_batches);

    SplitStorage splits(scratch / "splits");
    TRY(tabulate_splits(tree, root, splits, options));
    TRY(splits.save_index());

    TRY(const bvh::Reference wide_root, write_wide_nodes(splits, tree, root, output, options));
    TRY(output.save_index());
    return wide_root;
}

}