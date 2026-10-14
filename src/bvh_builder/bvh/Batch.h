#pragma once

#include <cstddef>
#include <limits>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include <libassert/assert.hpp>

#include "mem.h"
#include "range/Range.h"
#include "zpp_bits_utils.h"

#include "bvh/Node.h"
#include "bvh/NodeInput.h"
#include "bvh/NodeView.h"
#include "bvh/Reference.h"
#include "dag/ClusterInfo.h"

namespace bvh {

class Batch {
public:
    static constexpr BatchIndex UNBOUND_INDEX = std::numeric_limits<BatchIndex>::max();

    friend zpp::bits::access;
    explicit Batch(const BatchIndex index) : _index(index) {}

    static Batch make_unbound() {
        return Batch(UNBOUND_INDEX);
    }

    bool operator==(const Batch &) const = default;

    // How many bytes this batch occupies.
    friend size_t size_bytes(const Batch &batch) {
        return sizeof_val(batch.index(), batch.nodes(), batch.clusters(), batch.children());
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

    std::span<const dag::ClusterInfo> clusters() const {
        return this->_clusters;
    }
    std::span<const dag::ClusterInfo> clusters(const NodeIndex start, const NodeIndex count) const {
        DEBUG_ASSERT(start + count <= this->_clusters.size());
        return std::span<const dag::ClusterInfo>{this->_clusters}.subspan(start, count);
    }

    NodeView resolve(const Node &node) const {
        if (node.is_leaf()) {
            return NodeView{node.header(), this->clusters(node.start(), node.count())};
        } else {
            return NodeView{node.header(), this->children(node.start(), node.count())};
        }
    }

    // Calls whichever of `visitors` matches what `node` holds.
    template <typename... F>
    decltype(auto) visit(const Node &node, F &&...visitors) const {
        return this->resolve(node).visit(std::forward<F>(visitors)...);
    }

    // Stores `node` in this batch and returns its index.
    template <std::ranges::input_range R>
    NodeIndex add_node(const InnerInput<R> &node) {
        const Range<NodeIndex> children = append_all(this->_children, node.children);

        ASSERT(!children.is_empty());
        return this->push_node(Node(node.header, children.start, children.size(), false));
    }

    template <std::ranges::input_range R>
    NodeIndex add_node(const LeafInput<R> &node) {
        const Range<NodeIndex> clusters = append_all(this->_clusters, node.clusters);

        DEBUG_ASSERT(!clusters.is_empty());
        return this->push_node(Node(node.header, clusters.start, clusters.size(), true));
    }

    NodeIndex add_node(const NodeView &node) {
        return node.visit(
            [&](const NodeView::Children children) {
                return this->add_node(InnerInput{node.header(), children});
            },
            [&](const NodeView::Clusters clusters) {
                return this->add_node(LeafInput{node.header(), clusters});
            });
    }

private:
    // Appends `items` to `target` and returns the indices they occupy.
    template <typename T, std::ranges::input_range R>
    static Range<NodeIndex> append_all(std::vector<T> &target, const R &items) {
        const size_t start = target.size();
        target.append_range(items);
        return Range<NodeIndex>(Range(start, target.size()));
    }

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
    std::vector<dag::ClusterInfo> _clusters;
};

}
