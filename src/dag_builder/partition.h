#pragma once

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <unordered_map>
#include <vector>
#include <ranges>

#include "cluster.h"
#include "enumerate.h"
#include "range_utils.h"
#include "merge/clusters.h"
#include "Partitioning.h"
#include "partitioning/Provider.h"
#include "partitioning/provider/Meshopt.h"


struct PartitionOptions {
    uint32_t clusters_per_partition = 4;
};

namespace partitioning {

// The provider used when the caller names none.
inline const provider::Meshopt default_provider{};

} // namespace partitioning

inline Partitioning create_partitioning(const Clustering &clustering, const PartitionOptions options = {}, const partitioning::Provider &provider = partitioning::default_provider) {
    const uint32_t cluster_count = clustering.clusters.size();
    const uint32_t clusters_per_partition = options.clusters_per_partition;
    if (clusters_per_partition == 1 || cluster_count == 0) {
        std::vector<uint32_t> identity(cluster_count);
        std::iota(identity.begin(), identity.end(), 0);
        return Partitioning{cluster_count, clusters_per_partition, identity};
    }

    Partitioning partitioning = provider.partition(clustering, clusters_per_partition);
    remove_empty_partitions_inplace(partitioning);
    return partitioning;
}

inline Clustering partition(const Clustering &clustering, const PartitionOptions &options = {}, const partitioning::Provider &provider = partitioning::default_provider) {
    return merge_clusters(clustering, create_partitioning(clustering, options, provider));
}

