#pragma once

#include <cstddef>

#include "storage/batch_limits.h"

namespace sort {

struct Options {
    size_t max_batch_bytes = default_max_batch_bytes;
    size_t max_resident_bytes = default_max_resident_bytes;
};

}
