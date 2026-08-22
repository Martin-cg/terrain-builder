#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>

#include <tl/expected.hpp>

#include "io/Error.h"

#include "batch_limits.h"
#include "bvh/Storage.h"
#include "bvh/flat_traverse.h"
#include "bvh/layout.h"
#include "frontier/BatchWriter.h"
#include "frontier/Storage.h"
#include "frontier/layout.h"
#include "morton/code.h"
#include "bvh/earth.h"
#include "try.h"

namespace frontier {

// The entry a stored node enters the frontier as.
inline Entry make_entry(const bvh::Reference &ref, const bvh::Node &node, const bvh::Bounds &scene) {
    return Entry{
        .morton = morton::encode(scene, node.header),
        .header = node.header,
        .ref = ref,
    };
}

struct BuildOptions {
    size_t max_batch_bytes = default_max_batch_bytes;
    bvh::Bounds scene_bounds = bvh::earth_bounds();
};

// Writes every stored node as a set of frontier run batches.
inline tl::expected<void, io::Error> build_from_leaves(const bvh::Storage &nodes, Storage &output, const BuildOptions &options = {}) {
    BatchWriter writer(output, options.max_batch_bytes);

    // The walk stops on the first failed write, leaving it to report.
    TRY(bvh::flat_traverse(nodes, [&](const bvh::Reference &ref, const bvh::Node &node) {
        const auto entry = make_entry(ref, node, options.scene_bounds);
        return writer.write(entry);
    }));
    return writer.flush();
}

}
