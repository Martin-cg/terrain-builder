#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include "bvh/layout.h"
#include "morton/code.h"

// Layout
namespace frontier {

// Unmerged subtree roots on the active frontier. Carries the full header so a merged node can be
// written without reading its children back.
struct Entry {
    morton::Code morton;
    bvh::NodeHeader header;
    bvh::Reference ref;
};

// Admitted and held are recomputed per round from the limit, never stored.
struct Batch {
    std::vector<Entry> entries;
    // TODO: do we need any statistics here for admission acceleration or similar.
};

}
