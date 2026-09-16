#pragma once

#include <ranges>
#include <span>
#include <vector>

#include <tl/expected.hpp>

#include "io/Error.h"
#include "try.h"

#include "bvh/NodeCache.h"
#include "bvh/layout.h"
#include "visitor.h"

namespace bvh {

// Visits every node of the tree from `root` with the reference naming it in pre-order.
template <typename F>
[[nodiscard]] tl::expected<bool, io::Error> traverse_tree(NodeCache &nodes, const Reference &root, F &&v) {
    visitor::FallibleVisitor<io::Error, F &> visitor(v);

    std::vector<Reference> stack{root};
    // The children of the node being visited, copied out of its batch so that reads the visitor makes through `nodes` cannot pull them away.
    std::vector<Reference> children;

    while (!stack.empty()) {
        const Reference reference = stack.back();
        stack.pop_back();

        TRY(const Batch &batch, nodes.read_batch(reference.batch));
        const Node &node = batch.node(reference.node);
        const NodeView view = batch.visit(
            node,
            [&](const dag::Id &id) {
                return NodeView{node.header, id};
            },
            [&](const std::span<const Reference> node_children) {
                children.assign_range(node_children);
                return NodeView{node.header, std::span<const Reference>(children)};
            });

        TRY(const bool keep_going, visitor(reference, view));
        if (!keep_going) {
            return false;
        }

        // Pushed in reverse so that they come off the stack in stored order.
        if (view.is_inner()) {
            stack.append_range(children | std::views::reverse);
        }
    }

    // We walked to the end
    return true;
}

}
