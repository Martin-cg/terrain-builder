#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include <radix/geometry.h>

#include "zpp_bits_utils.h"

#include "dag_id.h"

// Layout
namespace bvh {
using Bounds = radix::geometry::Aabb3d;
using BatchIndex = uint32_t;
using NodeIndex = uint32_t;

struct Reference {
    BatchIndex batch;
    NodeIndex node;
};

struct NodeHeader {
    // Conservative error interval over this node's subtree.
    double min_child_error;
    double max_parent_error;
    uint64_t triangle_count;
    Bounds bounds;
};

struct Node {
    using Header = NodeHeader;

    NodeHeader header;
    uint32_t child_start;
    uint32_t child_count; // 0: leaf, child_start indexes Batch::leaves
};

struct Batch {
    BatchIndex index = std::numeric_limits<BatchIndex>::max();
    std::vector<Node> nodes;
    std::vector<Reference> children;
    std::vector<dag::Id> leaves;
};

}
