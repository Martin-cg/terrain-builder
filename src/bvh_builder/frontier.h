#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "bvh.h"
#include "morton.h"

// Layout
namespace frontier {

// Unmerged subtree roots on the active frontier. Carries the full header so a merged node can be
// written without reading its children back.
struct Entry {
    MortonCode morton;
    bvh::NodeHeader header;
    bvh::Reference ref;
};

// Admitted and held are recomputed per round from the limit, never stored.
struct Batch {
    uint32_t index = std::numeric_limits<uint32_t>::max();
    std::vector<Entry> entries;
    // TODO: do we need any statistics here for admission acceleration or similar.
};

}
