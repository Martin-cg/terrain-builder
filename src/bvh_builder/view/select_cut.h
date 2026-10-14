#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include "error/Result.h"
#include "error/try.h"
#include "io/Error.h"

#include "bvh/NodeCache.h"
#include "bvh/NodeHeader.h"
#include "bvh/NodeView.h"
#include "bvh/Reference.h"
#include "bvh/traverse_tree.h"
#include "dag/ClusterInfo.h"
#include "view/Camera.h"
#include "view/CutRule.h"
#include "view/Frustum.h"

namespace view {

// The cut selected for a camera, with the work it took to find.
struct Selection {
    // Every cluster on the cut and in view, sorted.
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
        const bool is_in_view = intersects(header.tight_bounds, frustum);

        return node.visit(
            [&](const std::span<const dag::ClusterInfo> clusters) {
                selection.leaves_tested++;
                if (!is_in_view || !should_visit_leaf(header, camera, rule)) {
                    return bvh::Control::Continue;
                }
                // A cluster shows while the group that produced it is small enough and the group listing it is not.
                for (const dag::ClusterInfo &cluster : clusters) {
                    if (is_small_enough_to_draw(cluster.error, cluster.lod_bounds, camera, rule) && intersects(cluster.tight_bounds, frustum)) {
                        selection.clusters.push_back(cluster.id);
                    }
                }
                return bvh::Control::Continue;
            },
            [&](const std::span<const bvh::Reference>) {
                selection.inner_nodes_tested++;
                if (is_in_view && !is_small_enough_to_draw(header.max_parent_error, header.lod_bounds, camera, rule)) {
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
