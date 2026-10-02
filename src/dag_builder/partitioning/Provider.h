#pragma once

#include <cstdint>

#include "Partitioning.h"
#include "cluster.h"

namespace partitioning {

// Assigns each cluster of a clustering to a partition.
class Provider {
public:
    virtual ~Provider() = default;

    virtual Partitioning partition(const Clustering &clustering, uint32_t clusters_per_partition) const = 0;
};

} // namespace partitioning
