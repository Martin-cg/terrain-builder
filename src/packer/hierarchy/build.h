#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include <libassert/assert.hpp>

#include "PageId.h"
#include "bvh/layout.h"
#include "containers/SegmentedBuffer.h"
#include "enumerate.h"
#include "format.h"
#include "log.h"
#include "numeric/checked_cast.h"
#include "quantize.h"
#include "variant_utils.h"
#include "vector_utils.h"

namespace packer::hierarchy {

class PageBuilder;

// Adds children to one node of a page being built.
class NodeWriter {
public:
    // Adds a child that is a node of this page, and returns its writer.
    NodeWriter push_local(const bvh::NodeHeader &header);
    // Adds a child that is a root of another page.
    void push_external(const bvh::NodeHeader &header, ExternalChild target);
    // Adds a child that is a cluster.
    void push_leaf(const bvh::NodeHeader &header, LeafChild target);

private:
    friend class PageBuilder;

    NodeWriter(PageBuilder *builder, const uint32_t parent) : _builder(builder), _parent(parent) {}

    PageBuilder *_builder;
    uint32_t _parent;
};

// Collects a hierarchy page in any order and lays it out the way the decoder reads it back.
class PageBuilder {
public:
    // Adds a root, numbered in the order roots are added, and returns its writer.
    NodeWriter push_root(const bvh::Bounds &bounds) {
        const GridBox box = quantize_bounds(bounds);
        const Frame frame = derive_frame(box);
        const uint32_t root = this->add_node(frame);
        this->_roots.push_back(root);
        return NodeWriter(this, root);
    }

    // The finished page.
    [[nodiscard]] EncodedPage finish() && {
        DEBUG_ASSERT(std::in_range<uint16_t>(this->_frames.size()));

        const SegmentedBuffer<uint32_t, uint32_t> children_of_node = this->group_children_by_parent();
        const std::vector<uint32_t> node_order = this->order_nodes_breadth_first(children_of_node);

        EncodedPage page;
        page.pages = this->collect_pages();
        DEBUG_ASSERT(std::in_range<uint16_t>(page.pages.size()));

        const auto find_slot = [&page](const PageId &id) {
            const auto it = std::ranges::lower_bound(page.pages, id);
            return debug_checked_cast<uint16_t>(std::distance(page.pages.begin(), it));
        };

        for (const uint32_t root : this->_roots) {
            page.root_frames.push_back(this->_frames[root]);
        }

        for (const uint32_t node : node_order) {
            const std::span<const uint32_t> children = children_of_node.segment(node);
            page.child_counts.push_back(debug_checked_cast<uint8_t>(children.size()));

            for (const uint32_t index : children) {
                const PendingChild &child = this->_children[index];
                page.children.push_back(child.record);

                match(
                    child.target,
                    [](const PendingLocal &) {},
                    [&](const ExternalChild &external) {
                        page.page_slots.push_back(find_slot(external.page));
                        page.local_indices.push_back(external.root);
                    },
                    [&](const LeafChild &leaf) {
                        page.page_slots.push_back(find_slot(leaf.page));
                        page.local_indices.push_back(leaf.cluster);
                        page.texture_page_slots.push_back(find_slot(leaf.texture_page));
                        page.texture_local_indices.push_back(leaf.texture);
                    });
            }
        }
        return page;
    }

private:
    friend class NodeWriter;

    // A node of the same page, by the order nodes were added in.
    struct PendingLocal {
        uint32_t node_index = 0;
    };

    using PendingTarget = std::variant<PendingLocal, ExternalChild, LeafChild>;

    // A pushed child, already encoded, with what it refers to.
    struct PendingChild {
        uint32_t parent = 0;
        EncodedChild record;
        PendingTarget target;
    };

    [[nodiscard]] uint32_t add_node(const Frame &frame) {
        this->_frames.push_back(frame);
        return debug_checked_cast<uint32_t>(this->_frames.size() - 1);
    }

    // Encodes a child of `parent` that refers to `target` and adds it.
    const EncodedChild &push_child(const uint32_t parent, const bvh::NodeHeader &header, const PendingTarget &target) {
        const ChildKind kind = match(
            target,
            [](const PendingLocal &) { return ChildKind::Local; },
            [](const ExternalChild &) { return ChildKind::External; },
            [](const LeafChild &) { return ChildKind::Leaf; });
        const EncodedChild record{
            .box = quantize_box(this->_frames[parent], quantize_bounds(header.bounds)),
            .min_error = quantize_error(header.min_child_error),
            .max_parent_error = quantize_error(header.max_parent_error),
            .kind = kind,
        };
        this->_children.push_back({.parent = parent, .record = record, .target = target});
        return this->_children.back().record;
    }

    NodeWriter push_local(const uint32_t parent, const bvh::NodeHeader &header) {
        // The child refers to the node it adds, which takes the next index.
        const PendingLocal target{.node_index = debug_checked_cast<uint32_t>(this->_frames.size())};
        const EncodedChild &record = this->push_child(parent, header, target);
        const GridBox decoded_box = dequantize_box(this->_frames[parent], record.box);
        const uint32_t node_index = this->add_node(derive_frame(decoded_box));
        DEBUG_ASSERT(node_index == target.node_index);
        return NodeWriter(this, node_index);
    }

    void push_external(const uint32_t parent, const bvh::NodeHeader &header, const ExternalChild target) {
        this->push_child(parent, header, target);
    }

    void push_leaf(const uint32_t parent, const bvh::NodeHeader &header, const LeafChild target) {
        const EncodedChild &record = this->push_child(parent, header, target);
        DEBUG_ASSERT(record.min_error <= record.max_parent_error);
        if (record.min_error == record.max_parent_error) {
            LOG_WARN_BACKOFF("A leaf has an empty error interval at {}", record.min_error);
        }
    }

    // The children of every node, as indices into `_children` in push order.
    [[nodiscard]] SegmentedBuffer<uint32_t, uint32_t> group_children_by_parent() const {
        std::vector<uint32_t> child_counts(this->_frames.size(), 0);
        for (const PendingChild &child : this->_children) {
            child_counts[child.parent]++;
        }

        SegmentedBuffer<uint32_t, uint32_t> children_of_node;
        children_of_node.init(child_counts);

        std::vector<uint32_t> filled(this->_frames.size(), 0);
        for (const auto [index, child] : enumerate<uint32_t>(this->_children)) {
            children_of_node(child.parent, filled[child.parent]) = index;
            filled[child.parent]++;
        }
        return children_of_node;
    }

    // Every node in page order: the roots, then each local child in the order a breadth-first walk meets it.
    [[nodiscard]] std::vector<uint32_t> order_nodes_breadth_first(const SegmentedBuffer<uint32_t, uint32_t> &children_of_node) const {
        std::vector<uint32_t> node_order = this->_roots;
        for (size_t i = 0; i < node_order.size(); i++) {
            for (const uint32_t index : children_of_node.segment(node_order[i])) {
                if (const PendingLocal *local = std::get_if<PendingLocal>(&this->_children[index].target)) {
                    node_order.push_back(local->node_index);
                }
            }
        }
        DEBUG_ASSERT(node_order.size() == this->_frames.size());
        return node_order;
    }

    // Every referenced page once, sorted so a slot can be found by binary search.
    [[nodiscard]] std::vector<PageId> collect_pages() const {
        std::vector<PageId> pages;
        for (const PendingChild &child : this->_children) {
            match(
                child.target,
                [](const PendingLocal &) {},
                [&](const ExternalChild &external) { pages.push_back(external.page); },
                [&](const LeafChild &leaf) {
                    pages.push_back(leaf.page);
                    pages.push_back(leaf.texture_page);
                });
        }
        dedup_by_sort(pages);
        return pages;
    }

    std::vector<Frame> _frames;
    std::vector<uint32_t> _roots;
    std::vector<PendingChild> _children;
};

inline NodeWriter NodeWriter::push_local(const bvh::NodeHeader &header) {
    return this->_builder->push_local(this->_parent, header);
}

inline void NodeWriter::push_external(const bvh::NodeHeader &header, const ExternalChild target) {
    this->_builder->push_external(this->_parent, header, target);
}

inline void NodeWriter::push_leaf(const bvh::NodeHeader &header, const LeafChild target) {
    this->_builder->push_leaf(this->_parent, header, target);
}

}
