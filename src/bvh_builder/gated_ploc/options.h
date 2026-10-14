#pragma once

#include <cstddef>

#include "build/merge_cost.h"
#include "bvh/earth.h"
#include "bvh/NodeHeader.h"
#include "storage/batch_limits.h"

namespace gated_ploc {

struct AdmissionOptions {
    // How many entries share one admission limit.
    size_t region_size = 1 << 16;
    // The order statistic a region measures its small nodes by.
    double quantile = 0.33;
    // How many times the extent at `quantile` a node may measure and still merge.
    double tolerance = 3.0;
};

struct Options {
    size_t search_radius = 16;
    double error_weight = build::DEFAULT_ERROR_WEIGHT;
    bvh::Bounds scene_bounds = bvh::earth_bounds();
    AdmissionOptions admission = {};

    size_t chunk_size = 1 << 15;
    size_t chunk_count = 32;
    size_t worker_count = 8;
    size_t max_batch_bytes = DEFAULT_MAX_BATCH_BYTES;
    size_t max_resident_bytes = DEFAULT_MAX_RESIDENT_BYTES;
};

} // namespace gated_ploc
