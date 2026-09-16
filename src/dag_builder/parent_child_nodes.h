#pragma once

#include <radix/geometry.h>

#include "geometry/geometry.h"
#include "octree/Id.h"
#include "octree/IdRect.h"
#include "octree/OddLevelShifted.h"

namespace dag {

// The region that may hold clusters belonging to the given node. Reaches past its own bounds, since ownership is based on group centroids.
inline radix::geometry::Aabb3d find_cluster_search_bounds(
    const octree::Id &target_id,
    const octree::OddLevelShifted &shifted_space) {
    // We cannot rely on the intersecting nodes on the next level, since some neighbours clusters may has a centroid in this node's bounds. This can happen when the groups is split at the end of the pipeline.
    const radix::geometry::Aabb3d padded_bounds = geometry::pad_bounds_relative(shifted_space.get_node_bounds(target_id), 0.5 - 1e-6);
    return radix::geometry::intersection(padded_bounds, shifted_space.bounds());
}

// Nodes one level finer that may hold data belonging to the given node.
inline octree::IdRect find_possible_child_nodes(
    const octree::Id &parent,
    const octree::OddLevelShifted &shifted_space) {
    const radix::geometry::Aabb3d search_bounds = find_cluster_search_bounds(parent, shifted_space);
    return shifted_space.get_intersecting_nodes_on_level(search_bounds, parent.level() + 1);
}

// The nodes one level coarser whose cluster search bounds reach `child`. Empty at level 0.
inline octree::IdRect find_possible_parent_nodes(
    const octree::Id &child,
    const octree::OddLevelShifted &shifted_space) {
    if (child.level() == 0) {
        return {};
    }

    // A search reaches less than one node past its node, so a parent is at most one node away from those the child overlaps.
    return shifted_space.get_intersecting_nodes_on_level(child, child.level() - 1).grow(1);
}

} // namespace dag
