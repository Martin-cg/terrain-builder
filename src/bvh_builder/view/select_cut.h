#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include "error/Result.h"
#include "error/try.h"
#include "io/Error.h"

#include "bvh/NodeCache.h"
#include "bvh/layout.h"
#include "bvh/traverse_tree.h"
#include "dag_id.h"
#include "view/Camera.h"
#include "view/CutRule.h"
#include "view/Frustum.h"

namespace view {

// The cut selected for a camera, with the work it took to find.
struct Selection {
    // The selected clusters, sorted.
    std::vector<dag::Id> clusters;
    // How many inner nodes were checked.
    uint64_t inner_nodes_tested;
    // How many leaves were checked.
    uint64_t leaves_tested;

    bool operator==(const Selection &) const = default;
};

// Selects the clusters of the cut for `camera` under `rule` that it has in view, from the tree at `root`.
// Skips every subtree that is out of view or cannot hold any of the cut.
// Fails if a batch cannot be read.
[[nodiscard]]
inline alp::Result<Selection, io::Error> select_cut(bvh::NodeCache &nodes, const bvh::Reference &root, const Camera &camera, const CutRule &rule) {
    const Frustum frustum = Frustum::from_camera(camera);

    Selection selection{};
    const auto visit = [&](const bvh::Reference &, const bvh::NodeView &node) {
        const bvh::NodeHeader &header = node.header();
        const bool is_in_view = intersects(header.bounds, frustum);

        return node.visit(
            [&](const dag::Id &id) {
                selection.leaves_tested++;
                if (is_in_view && is_on_cut(header, camera, rule)) {
                    selection.clusters.push_back(id);
                }
                return bvh::Control::Continue;
            },
            [&](const std::span<const bvh::Reference>) {
                selection.inner_nodes_tested++;
                if (is_in_view && is_cut_below(header, camera, rule)) {
                    return bvh::Control::Continue;
                }
                return bvh::Control::Prune;
            });
    };
    ALP_TRY(bvh::traverse_tree(nodes, root, visit));

    std::ranges::sort(selection.clusters);
    return selection;
}

}
