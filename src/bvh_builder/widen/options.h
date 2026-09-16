#pragma once

#include <cstddef>
#include <cstdint>

#include "storage/batch_limits.h"

namespace widen {

// The most children a node may end up with.
constexpr uint32_t max_arity = 127;

struct Options {
    // The most children a node may end up with (2 <= x <= max_arity).
    uint32_t arity = 8;
    // The weight the error interval carries against the extent.
    double error_weight = 1.0;
    size_t max_batch_bytes = default_max_batch_bytes;
    size_t max_resident_bytes = default_max_resident_bytes;
};

}