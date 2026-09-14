#pragma once

#include <cstdint>
#include <span>

#include "cluster.h"

// One cluster, named by index in the clustering it belongs to.
struct ClusterView {
    const Clustering &clustering;
    uint32_t cluster = 0; // index into clustering.clusters
};

// A subset of one clustering's clusters, named by index.
struct ClusteringView {
    const Clustering &clustering;
    std::span<const uint32_t> clusters;
};