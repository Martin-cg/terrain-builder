#pragma once

#include <ranges>
#include <span>
#include <type_traits>
#include <vector>

#include "error/Result.h"
#include "error/ResultFn.h"
#include "error/try.h"
#include "io/Error.h"

#include "bvh/NodeCache.h"
#include "bvh/layout.h"

namespace bvh {

// What a traversal does after visiting a node.
class Control {
public:
    static const Control Continue;
    static const Control Prune;
    static const Control Break;

    // Continues on true and breaks off on false.
    constexpr Control(const bool keep_going) : _action(Action::Break) {
        if (keep_going) {
            this->_action = Action::Continue;
        }
    }

    bool operator==(const Control &) const = default;

private:
    enum class Action {
        Continue,
        Prune,
        Break
    };

    constexpr explicit Control(const Action action) : _action(action) {}

    Action _action;
};

inline constexpr Control Control::Continue{Action::Continue};
inline constexpr Control Control::Prune{Action::Prune};
inline constexpr Control Control::Break{Action::Break};

// Visits the tree from `root` in pre-order, passing each node with the reference naming it and skipping the children of every node `v` prunes.
// False if `v` broke off the walk before its end.
// Fails with `E` if a batch cannot be read or `v` fails.
template <typename E = io::Error, typename F>
[[nodiscard]]
alp::Result<bool, E> traverse_tree(NodeCache &nodes, const Reference &root, F &&v) {
    alp::ResultFn<E, F &> visit(v);

    std::vector<Reference> stack{root};
    // The children of the node being visited, copied out of its batch so that reads the visitor makes through `nodes` cannot pull them away.
    std::vector<Reference> children;

    while (!stack.empty()) {
        const Reference reference = stack.back();
        stack.pop_back();

        ALP_TRY(const Batch &batch, nodes.read_batch(reference.batch));
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

        // A visitor that returns nothing always continues.
        Control control = Control::Continue;
        if constexpr (std::is_void_v<typename decltype(visit(reference, view))::value_type>) {
            ALP_TRY(visit(reference, view));
        } else {
            ALP_TRY(control, visit(reference, view));
        }

        if (control == Control::Break) {
            return false;
        }

        // Pushed in reverse so that they come off the stack in stored order.
        if (view.is_inner() && control == Control::Continue) {
            stack.append_range(children | std::views::reverse);
        }
    }

    // We walked to the end
    return true;
}

}
