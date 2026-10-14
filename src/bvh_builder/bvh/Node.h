#pragma once

#include <utility>

#include <libassert/assert.hpp>

#include "zpp_bits_utils.h"

#include "bvh/NodeHeader.h"
#include "bvh/Reference.h"

namespace bvh {

class Batch;

// A node of the bvh, whose contents its batch resolves.
class Node {
public:
    using Header = NodeHeader;

    // zpp bits wants a default ctor
    Node() = default;

    bool operator==(const Node &) const = default;

    const NodeHeader &header() const {
        return this->_header;
    }

    bool is_leaf() const noexcept {
        return (this->_packed_count & LEAF_BIT) != 0;
    }
    bool is_inner() const noexcept {
        return !this->is_leaf();
    }

private:
    friend Batch;
    friend zpp::bits::access;
    using serialize = zpp::bits::members<3>;

    // Marks a leaf in the packed count, since both kinds share the start and the count.
    static constexpr NodeIndex LEAF_BIT = NodeIndex{1} << 31;

    Node(NodeHeader header, const NodeIndex start, const NodeIndex count, const bool is_leaf)
        : _header(std::move(header)), _start(start), _packed_count(is_leaf ? count | LEAF_BIT : count) {
        DEBUG_ASSERT(count < LEAF_BIT);
    }

    NodeIndex start() const {
        return this->_start;
    }

    NodeIndex count() const {
        return this->_packed_count & ~LEAF_BIT;
    }

    NodeHeader _header;
    NodeIndex _start;
    NodeIndex _packed_count;
};

}
