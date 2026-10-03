#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <variant>
#include <vector>

#include <libassert/assert.hpp>
#include <radix/geometry.h>

#include "mem.h"
#include "variant_utils.h"
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

    auto operator<=>(const Reference &) const = default;
};

struct NodeHeader {
    // Conservative error interval over this node's subtree.
    double min_child_error;
    double max_parent_error;
    Bounds bounds;

    bool operator==(const NodeHeader &) const = default;
};

struct Node {
    using Header = NodeHeader;

    NodeHeader header;
    NodeIndex child_start;
    NodeIndex child_count; // if 0 -> leaf & child_start indexes Batch::leaves

    bool operator==(const Node &) const = default;

    bool is_leaf() const noexcept {
        return this->child_count == 0;
    }
    bool is_inner() const noexcept {
        return !this->is_leaf();
    }
};

class NodeView {
public:
    using Children = std::span<const Reference>;
    using Contents = std::variant<Children, dag::Id>;

    NodeView(const NodeHeader &header, const Contents contents)
        : _header(header), _contents(contents) {}

    const NodeHeader &header() const {
        return this->_header;
    }

    bool is_leaf() const {
        return std::holds_alternative<dag::Id>(this->_contents);
    }

    bool is_inner() const {
        return !this->is_leaf();
    }

    Children children() const {
        DEBUG_ASSERT(this->is_inner());
        return std::get<Children>(this->_contents);
    }

    dag::Id leaf() const {
        DEBUG_ASSERT(this->is_leaf());
        return std::get<dag::Id>(this->_contents);
    }

    // Calls whichever of `visitors` matches what the node holds.
    template <typename... F>
    decltype(auto) visit(F &&...visitors) const {
        return match(this->_contents, std::forward<F>(visitors)...);
    }

private:
    NodeHeader _header;
    Contents _contents;
};

class Batch {
public:
    static constexpr BatchIndex unbound_index = std::numeric_limits<BatchIndex>::max();

    friend zpp::bits::access;
    explicit Batch(const BatchIndex index) : _index(index) {}

    static Batch make_unbound() {
        return Batch(unbound_index);
    }

    bool operator==(const Batch &) const = default;

    // How many bytes this batch occupies.
    friend size_t size_bytes(const Batch &batch) {
        return sizeof_val(batch.index(), batch.nodes(), batch.leaves(), batch.children());
    }

    BatchIndex index() const {
        return this->_index;
    }

    std::span<const Node> nodes() const {
        return this->_nodes;
    }
    const Node &node(const NodeIndex index) const {
        DEBUG_ASSERT(index < this->_nodes.size());
        return this->_nodes[index];
    }

    std::span<const Reference> children() const {
        return this->_children;
    }
    std::span<const Reference> children(const NodeIndex start, const NodeIndex count) const {
        DEBUG_ASSERT(start + count <= this->_children.size());
        return std::span<const Reference>{this->_children}.subspan(start, count);
    }

    std::span<const dag::Id> leaves() const {
        return this->_leaves;
    }
    const dag::Id& leaf(const NodeIndex index) const {
        DEBUG_ASSERT(index < this->_leaves.size());
        return this->_leaves[index];
    }

    NodeView resolve(const Node &node) const {
        if (node.is_leaf()) {
            return NodeView{node.header, this->leaf(node.child_start)};
        } else {
            return NodeView{node.header, this->children(node.child_start, node.child_count)};
        }
    }

    // Calls whichever of `visitors` matches what `node` holds.
    template <typename... F>
    decltype(auto) visit(const Node &node, F &&...visitors) const {
        return this->resolve(node).visit(std::forward<F>(visitors)...);
    }

    NodeIndex add_leaf_node(NodeHeader header, const dag::Id id) {
        const auto leaf_index = static_cast<NodeIndex>(this->_leaves.size());
        this->_leaves.push_back(id);

        return this->push_node(Node{
            .header = std::move(header),
            .child_start = leaf_index,
            .child_count = 0,
        });
    }

    template <std::ranges::input_range R>
    NodeIndex add_inner_node(NodeHeader header, R &&children) {
        const auto child_start = static_cast<NodeIndex>(this->_children.size());
        this->_children.append_range(std::forward<R>(children));
        const auto child_count = static_cast<NodeIndex>(this->_children.size()) - child_start;

        ASSERT(child_count > 0);
        return this->push_node(Node{
            .header = std::move(header),
            .child_start = child_start,
            .child_count = child_count,
        });
    }

private:
    // zpp bits wants a default ctor
    friend zpp::bits::access;
    using serialize = zpp::bits::members<4>;
    Batch() = default;

    NodeIndex push_node(Node node) {
        const auto index = static_cast<NodeIndex>(this->_nodes.size());
        this->_nodes.push_back(std::move(node));
        return index;
    }

    BatchIndex _index;
    std::vector<Node> _nodes;
    std::vector<Reference> _children;
    std::vector<dag::Id> _leaves;
};

}
